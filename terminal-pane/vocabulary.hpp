// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TERMINAL_PANE_VOCABULARY_HPP
#define ZENGINE_TERMINAL_PANE_VOCABULARY_HPP

// The Terminal pane's durable names: the office it holds, the one pane it offers, the action
// ids its keys answer to, and the state a same-shape reload keeps. The office is neither the
// host's `zengine.workshop` (admission refuses a pane offered by its holder, WL-CAT-03) nor the
// participant's: a maker types here and the participant speaks there, two identities that stay
// two. No retired `PaneRef` converts: the Terminal was never a panel a saved desk could name.
// Workshop law: agents/workshop/terminal-pane.md

// The five action ids (`terminal.submit`, `.back`, `.previous`, `.next`, `.complete`) keep the
// spellings and keys makers' overrides name; Workshop declares none of them any more.

#include <zen/weave/shape.hpp>

#include <cstdint>

namespace zengine::terminal_pane {

/// THE OFFICE THIS PANE HOLDS -- the durable half of its `PaneRef`, and the only address
/// anything reaches it by. A ROLE, so Workshop's `PaneRoom` finds whoever holds it rather
/// than the incarnation that offered first.
inline constexpr const char* kTerminalPaneRole = "zengine.terminal";

/// THE PANE KEY, in this office's namespace.
inline constexpr const char* kTerminalPane = "terminal";

/// THE TWO LINES A MAKER READS ABOUT THIS PANE -- its name in the Pane Manager's list, its
/// summary in Info -- and what Workshop's pane header says after the office.
inline constexpr const char* kTerminalPaneName = "Terminal";
inline constexpr const char* kTerminalPaneSummary = "talk to the weaves on this bus";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable the way the
/// office is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kTerminalPaneStem = "zengine-terminal-pane";

// ---- THE ACTIONS THE PANE DECLARES (`PaneActions`, workshop/pane_vocabulary.hpp) ---------

inline constexpr const char* kActionSubmit = "terminal.submit";     ///< author the line, or lock a recall
inline constexpr const char* kActionBack = "terminal.back";         ///< back: recall, list, line, desk
inline constexpr const char* kActionUp = "terminal.previous";       ///< older command, or list up
inline constexpr const char* kActionDown = "terminal.next";         ///< newer command, or list down
inline constexpr const char* kActionComplete = "terminal.complete"; ///< "help me here", or lock a recall

/// THE RECORD'S FOUR READING KEYS -- a page of the view each way, the oldest kept row and the newest.
/// On Ctrl with the arrows, Home and End, which every backend reports (the vocabulary has no page
/// keys) and which leave plain Up and Down to history and completion. The line keeps Home and End.
inline constexpr const char* kActionScrollUp = "terminal.scroll-up";
inline constexpr const char* kActionScrollDown = "terminal.scroll-down";
inline constexpr const char* kActionOldest = "terminal.oldest";
inline constexpr const char* kActionNewest = "terminal.newest";

/// The state a same-shape reload keeps: the line the maker is half-way through typing, and only
/// its text -- the new image places the caret at the end, where the completer needs it. Kept, not
/// dropped as Info's and the Builder's drafts are, for the effort it holds and its inertness
/// until an explicit submit (agents/decisions/the-terminal-is-a-participant.md); a kept line
/// naming a role reaches whoever holds it at submit. The transcript and the completion are
/// re-said or re-asked, never kept.
struct TerminalPaneState {
    std::string line; ///< what the maker had typed and not yet submitted
    ZEN_SHAPE(TerminalPaneState, 1, ZEN_FIELD(line));
};

} // namespace zengine::terminal_pane

#endif // ZENGINE_TERMINAL_PANE_VOCABULARY_HPP
