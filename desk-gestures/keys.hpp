// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESK_GESTURES_KEYS_HPP
#define ZENGINE_DESK_GESTURES_KEYS_HPP

// Where the keys go, as a value: the routing chain from the few facts it reads, which a host
// gathers from what it holds. A function of its arguments, holding nothing, so the chain is proved
// here and a host proves only that it gathers the facts.

#include <cstdint>

namespace zengine::desk_gestures {

/// WHERE THE KEYBOARD CURRENTLY GOES -- the routing chain's branches, as values.
// WL-KEY-03 -- agents/workshop/keyboard.md
enum class KeyContext : std::uint8_t {
    kCommand,
    kNaming,
    kContext,
    kPane,
    kArrangePane,
    kArrangeDesk,
    kArrangeReset,
    kGlobal,
    kNoText,
    /// Everywhere, unless the pane holding the keyboard declared it owns this action
    /// (`PaneActionRow::supersedes`): declared by the pane, so it survives a weaver moving either
    /// row's key.
    kUnlessOwned,
};

/// THE FACTS THE ROUTING CHAIN READS, and nothing else: each is a mode a host holds open or not.
struct KeyModes {
    bool arranging = false;  ///< an arrangement is open, and not a pane being placed as it opened
    bool resetting = false;  ///< ...its reset prompt is up
    bool desk = false;       ///< ...it is the whole desk's, not one pane's
    bool menu = false;       ///< a contextual menu is open: the host's own, or a pane's
    bool naming = false;     ///< a layout's name line is open
    bool pane_keys = false;  ///< the pane the keys return to is a runtime pane, which takes them
};

/// THE CHAIN BELOW THE CONTEXTUAL SURFACE -- the branches a key falls to once no mode above
/// them claims it: what the keys would mean when the menu closes.
// WL-KEY-03 -- agents/workshop/keyboard.md
constexpr KeyContext keyboard_context_beneath_menu(const KeyModes& m) noexcept {
    if (m.naming) {
        return KeyContext::kNaming;
    }
    return m.pane_keys ? KeyContext::kPane : KeyContext::kCommand;
}

/// WHERE THE KEYBOARD GOES, AS ONE VALUE: each mode above the next. It is resolved from the modes
/// at every spend and stored nowhere, so a mode that closes stops being the answer with nothing
/// to clear.
// WL-KEY-03 -- agents/workshop/keyboard.md
// WL-ARR-14 -- agents/workshop/arrangement.md
constexpr KeyContext keyboard_context(const KeyModes& m) noexcept {
    if (m.arranging) {
        if (m.resetting) {
            return KeyContext::kArrangeReset;
        }
        return m.desk ? KeyContext::kArrangeDesk : KeyContext::kArrangePane;
    }
    // The contextual-action surface is a mode at the top of the modes' band, below the
    // arrangement scopes: it answers before a focused pane, or its navigation keys would leak
    // beneath it.
    if (m.menu) {
        return KeyContext::kContext;
    }
    return keyboard_context_beneath_menu(m);
}

/// Where the keys went to a list or to nothing, as against a place a weaver types into, which
/// keeps its own keys: the contexts in which the application's default rows are answered.
// WL-ARR-13, WL-ARR-14 -- agents/workshop/arrangement.md
// WL-DESK-02 -- agents/workshop/desktop.md
constexpr bool default_row_context(KeyContext c) noexcept {
    return c == KeyContext::kCommand;
}

} // namespace zengine::desk_gestures

#endif // ZENGINE_DESK_GESTURES_KEYS_HPP
