// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESK_GESTURES_MENUS_HPP
#define ZENGINE_DESK_GESTURES_MENUS_HPP

// A menu walked and chosen, and a pane's menu kept in custody, as transitions: functions of the
// rows a menu shows, where its cursor stands and the numbers its custody keeps. None holds state
// or authority: a host keeps the menu, reads the key or the refusal, calls these, and spends what
// they answer.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::desk_gestures {

// ---- a menu walked and chosen ---------------------------------------------------------------

/// The cursor, bounded by the population's size: the population is derived, so it can move
/// between a keystroke and a repaint.
constexpr std::size_t context_cursor_bound(std::size_t cursor, std::size_t population) noexcept {
    if (cursor < population) {
        return cursor;
    }
    return population == 0 ? 0 : population - 1;
}

/// THE CURSOR ONE ROW UP (`by` < 0) OR DOWN (`by` > 0) among `rows` shown rows, from where it
/// stands bounded: it stops at either end rather than wrapping, so a held key rests on the last
/// row.
constexpr std::size_t menu_cursor_moved(std::size_t cursor, std::size_t rows,
                                        std::int64_t by) noexcept {
    const std::size_t at = context_cursor_bound(cursor, rows);
    if (by < 0) {
        return at > 0 ? at - 1 : at;
    }
    if (by > 0) {
        return at + 1 < rows ? at + 1 : at;
    }
    return at;
}

/// What the back key does: the smaller thing, out of an open group, else out of the menu.
enum class MenuBack : std::uint8_t { kLeaveGroup, kClose };

constexpr MenuBack menu_back(std::string_view open_group) noexcept {
    return open_group.empty() ? MenuBack::kClose : MenuBack::kLeaveGroup;
}

/// WHERE THE CURSOR LANDS ON LEAVING THE GROUP `was`, among the top level's rows: on that group's
/// own row, so backing out reads as returning rather than starting over, else on the first row.
/// `Row` is any row with `is_group` and a `group` name.
template <class Row>
std::size_t cursor_on_group(const std::vector<Row>& top, std::string_view was) {
    for (std::size_t i = 0; i < top.size(); ++i) {
        if (top[i].is_group && was == std::string_view(top[i].group)) {
            return i;
        }
    }
    return 0;
}

/// What the row under the cursor names: nothing (the cursor is past the rows), a group to enter,
/// or an action to spend, by its position among the rows.
struct MenuChoice {
    enum class Kind : std::uint8_t { kNothing, kGroup, kAction };
    Kind kind = Kind::kNothing;
    std::size_t at = 0;
};

template <class Row>
MenuChoice menu_choice(const std::vector<Row>& rows, std::size_t cursor) {
    if (cursor >= rows.size()) {
        return MenuChoice{};
    }
    return MenuChoice{rows[cursor].is_group ? MenuChoice::Kind::kGroup : MenuChoice::Kind::kAction,
                      cursor};
}

// ---- a pane's own menu, in custody ----------------------------------------------------------

/// ONE PRESS THE WINDOW LANDED IN A PRESENTED MENU'S AREA: the window's number for it, which the
/// presenter's word about it cites, and the weaver's act it was.
struct MenuPress {
    std::uint64_t press = 0;
    std::uint64_t act = 0;
};

/// THE MOST PRESSES A PRESENTED MENU KEEPS, newest last: a choice cites one just made.
inline constexpr std::size_t kMaxMenuPresses = 16;

/// THE PRESSES A MENU KEEPS once `press` lands: newest last, and past `kMaxMenuPresses` the oldest
/// goes.
inline std::vector<MenuPress> kept_presses(std::vector<MenuPress> kept, MenuPress press) {
    kept.push_back(press);
    if (kept.size() > kMaxMenuPresses) {
        kept.erase(kept.begin());
    }
    return kept;
}

/// IS A REFUSAL OF `attempt` THIS MENU'S? Every sentence a host queues to the presenter's role
/// from the grant until the menu ends is about this menu, one menu at a time, and Loom numbers
/// attempts in the order they are queued: an attempt at or after the grant's (`first_attempt`) is
/// this menu's, and one before it an older menu's. A grant not yet queued (0) owns none.
constexpr bool menu_refusal_is_ours(std::uint64_t first_attempt, std::uint64_t attempt) noexcept {
    return first_attempt != 0 && attempt >= first_attempt;
}

/// A menu the host took off the screen whose requester may still be owed an answer: kept until
/// the withdrawal, an ask of the presenter's role, is answered or ends, and answered by the host,
/// unchosen, once, when no presenter could answer it.
// WL-CTX-10 -- agents/workshop/pane-menu.md
struct WithdrawnMenu {
    std::int64_t menu = 0;           ///< the grant's number
    std::string role;                ///< the requester, as Loom authenticated it on the ask
    std::string pane;
    std::string subject;
    std::uint64_t correlation = 0;   ///< the ask's number, which the answer echoes
    std::string why;                 ///< the host's words for ending it
    std::uint64_t first_attempt = 0; ///< its grant's queued attempt...
    std::uint64_t last_attempt = 0;  ///< ...through its withdrawal's: every sentence about it
};

/// WHICH KEPT WITHDRAWN MENU A REFUSAL OF `attempt` IS ABOUT: the first whose sentences, from its
/// grant through its withdrawal, include it, or none. The host answers that one and forgets it, so
/// a second refusal finds no menu to answer twice.
inline std::optional<std::size_t> withdrawn_refused(const std::vector<WithdrawnMenu>& kept,
                                                    std::uint64_t attempt) noexcept {
    for (std::size_t i = 0; i < kept.size(); ++i) {
        if (attempt >= kept[i].first_attempt && attempt <= kept[i].last_attempt) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace zengine::desk_gestures

#endif // ZENGINE_DESK_GESTURES_MENUS_HPP
