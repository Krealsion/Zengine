// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_CONTEXT_HPP
#define ZENGINE_WORKSHOP_CONTEXT_HPP

// WHAT CAN I DO WITH THIS? -- the contextual-action surface (CTX-0).
//
// A maker points at a thing and asks what can be done with it. Two laws bound everything
// in this file:
//
//   POINTING NAMES A SUBJECT FOR ONE REQUEST. SELECTION IS A STATE A MAKER ENTERED.
//   Opening this surface captures a temporary subject and changes no selection, no
//   keyboard candidate and no mode. The one deliberate exception is choosing Move or
//   Size, whose meaning IS entering ongoing interest -- and even they admit their
//   explicit target first and select only on acceptance.
//
//   OPEN REMEMBERS AN IDENTITY. SPEND RE-ASKS ITS OWNER.
//   The captured subject is only what a file could hold -- a `PaneRef`, an object id, or
//   nothing. Never a resolved rectangle, a catalog row, a kind handle or a row list:
//   every one of those is re-asked from its owner at the moment it is needed, which is
//   how a subject that has since disappeared is answered by the owner's own truthful
//   refusal instead of by a stale snapshot.
//
// THE DECLARATION BELOW NAMES ACTIONS AND HOLDS NO POWER -- `Condition::action`'s rule,
// one surface over. A row references a stable `kActionCatalog` id; it mints no identity,
// carries no callback, no `std::function`, no gesture (the keymap answers that), no label
// (the action's own declaration answers that) and no availability (the owner answers that
// at spend, in its own words). Which actions are MEANINGFUL for a kind of subject is a
// static fact and is all this table says; whether an exact operation would succeed on an
// exact subject right now is owner policy and never rides the paint path.
//
// It is a SEPARATE table rather than fields on `ActionRow` because the two are keyed
// differently: the action catalog is (action x keyboard context x gesture) and an action
// may own several rows there (`manage.done` owns three), while a menu must show an id
// once, under a subject and a heading.

#include "keymap.hpp"
#include "setup.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop {

/// THE SUBJECT KINDS a contextual request can truthfully name -- and there are exactly
/// three, because that is what Workshop's own resolvers can answer at a pointed position:
/// a pane it placed (`occupied_at` -> the setup row that resolves to that kind), an
/// authored document object (`object_at`), or nothing at all, which is the empty room and
/// is a real subject with no identity. Do not generalize these into a type-erased
/// container: each kind's identity already has its own owned type.
namespace context_subject {
inline constexpr std::int64_t kRoot = 0;   ///< the empty room / Workshop itself
inline constexpr std::int64_t kPane = 1;   ///< an arrangeable pane, by durable `PaneRef`
inline constexpr std::int64_t kObject = 2; ///< a document object, by minted identity
} // namespace context_subject

/// One subject kind as a declaration bit, so a row can be meaningful for several kinds
/// without a second row.
inline constexpr std::int64_t context_bit(std::int64_t subject) noexcept {
    return std::int64_t{1} << subject;
}

inline constexpr std::int64_t kOnRoot = context_bit(context_subject::kRoot);
inline constexpr std::int64_t kOnPane = context_bit(context_subject::kPane);
inline constexpr std::int64_t kOnObject = context_bit(context_subject::kObject);

/// THE SURFACE'S OWN STATE -- a mode in the picker's family: open, a captured subject,
/// which group level is showing, and a cursor. Session, emphatically not content, and it
/// holds an IDENTITY and a cursor, never a snapshot: no bounds, no rows, no resolved
/// handles. `pane` is read exactly when `subject == kPane` and `object` exactly when
/// `subject == kObject`; the other fields rest at their defaults.
struct ContextMenu {
    bool open = false;
    std::int64_t subject = context_subject::kRoot;
    PaneRef pane;
    std::int64_t object = 0;
    /// The open presentation group, "" for the top level. A group is its name -- it has
    /// no identity, no gesture and no dispatch arm, and an empty one is never shown.
    std::string group;
    std::size_t cursor = 0;
};

/// ONE DECLARATION: an action id, the subject kinds it is meaningful for, and the
/// presentation group it is offered under ("" = the menu's own top level). A REFERENCE,
/// never an identity -- see the header comment.
struct ContextRow {
    const char* action = "";
    std::int64_t subjects = 0;
    const char* group = "";
};

/// THE DECLARED FIRST POPULATIONS (CTX-0). Order inside a subject's group is
/// presentation priority, `kActionCatalog`'s own rule. Everything here is
/// Workshop-owned: a pane row acts on the RECTANGLE Workshop placed, never on a
/// provider's content, and no provider contributes a row (the pane seam has no shape for
/// one, deliberately).
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
    {"manage.move", kOnPane, ""},
    {"manage.size", kOnPane, ""},
    {"manage.front", kOnPane, "Arrange"},
    {"manage.back", kOnPane, "Arrange"},
    {"manage.raise", kOnPane, "Arrange"},
    {"manage.lower", kOnPane, "Arrange"},
    {"manage.reset-place", kOnPane, "Reset"},
    {"manage.reset-width", kOnPane, "Reset"},
    {"manage.reset-height", kOnPane, "Reset"},
    {"manage.remove", kOnPane, ""},
    // -- a document object -------------------------------------------------------------
    {"object.delete", kOnObject, ""},
};

inline constexpr std::size_t kContextCatalogCount =
    sizeof(kContextCatalog) / sizeof(kContextCatalog[0]);

// ---- The drift guard: every reference resolves, at compile time --------------------------
//
// `kPanelCatalog`'s own pattern: a stale or misspelled reference is a build failure, not a
// silently missing menu row. The comparison is spelled here because the catalog holds
// `const char*` and this must run in a constant expression.

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

/// THE POPULATION AT ONE LEVEL -- the one owner, spent by the painter, the cursor bound,
/// the keyboard's choose and the pointer's press (`picker_population`'s discipline: one
/// list, four consumers, or the cursor and the picture come to disagree about which index
/// means what).
///
/// At the top level, a declared group appears ONCE, as a group entry at the position of
/// its first member -- so an empty group disappears naturally, because a group entry only
/// exists where a member declared it. Inside a group, exactly that group's members
/// appear. Declaration order is presentation order throughout.
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
