// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TERMINAL_PANE_VOCABULARY_HPP
#define ZENGINE_TERMINAL_PANE_VOCABULARY_HPP

// The Terminal pane's durable names: the role it holds, the one pane it offers, the action
// ids its keys answer to, and the state a same-shape reload keeps. The role is neither the
// host's `zengine.workshop` (admission refuses a pane offered by its holder, WL-CAT-03) nor the
// participant's: a weaver types here and the participant speaks there, two identities that stay
// two. No saved desk names the Terminal as a built-in pane, so no `PaneRef` of one converts. The
// Terminal's laws are terminal/docs/terminal-laws.md.

// The five action ids (`terminal.submit`, `.back`, `.previous`, `.next`, `.complete`) keep the
// spellings and keys weavers' overrides name, and only this pane declares them.

#include <zen/weave/shape.hpp>

#include <cstdint>

namespace zengine::terminal_pane {

/// The role this pane holds -- the durable half of its `PaneRef`, and the only address
/// anything reaches it by. A role, so the window and Workshop find whoever holds it rather
/// than the incarnation that offered first.
inline constexpr const char* kTerminalPaneRole = "zengine.terminal";

/// The pane key, in this role's namespace.
inline constexpr const char* kTerminalPane = "terminal";

/// The two lines a weaver reads about this pane -- its name in the Pane Manager's list, its
/// summary in Info -- and what Workshop's pane header says after the role.
inline constexpr const char* kTerminalPaneName = "Terminal";
inline constexpr const char* kTerminalPaneSummary = "talk to the weaves on this bus";

/// The library stem a host boots. Not part of the pane protocol and not durable the way the
/// role is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kTerminalPaneStem = "zengine-terminal-pane";

// ---- The actions the pane declares (`PaneActions`, pane/vocabulary.hpp) ---------

/// Author the line, take the candidate the weaver moved the list's cursor to, or lock a recall.
inline constexpr const char* kActionSubmit = "terminal.submit";
/// Author the line as typed, whatever candidate the list's cursor stands on, or lock a recall.
inline constexpr const char* kActionSubmitEarly = "terminal.submit-early";
inline constexpr const char* kActionBack = "terminal.back";         ///< back: recall, list, line, desk
inline constexpr const char* kActionUp = "terminal.previous";       ///< older command, or list up
inline constexpr const char* kActionDown = "terminal.next";         ///< newer command, or list down
inline constexpr const char* kActionComplete = "terminal.complete"; ///< "help me here", or lock a recall

/// The settings the pane declares (`v2::PaneSettingsDeclared`, pane/settings.hpp): whether
/// its legend row shows in a layout, a flag that is on unless the layout keeps it off; how many rows
/// of the record a wheel notch reads; and the mark the line is typed after, one of a few marks.
inline constexpr const char* kSettingLegend = "legend";
inline constexpr const char* kSettingWheelRows = "wheel-rows";
inline constexpr const char* kSettingPrompt = "prompt";

/// The record's four reading keys -- a page of the view each way, the oldest kept row and the newest.
/// On Ctrl with the arrows, Home and End, which every backend reports (the vocabulary has no page
/// keys) and which leave plain Up and Down to history and completion. The line keeps Home and End.
inline constexpr const char* kActionScrollUp = "terminal.scroll-up";
inline constexpr const char* kActionScrollDown = "terminal.scroll-down";
inline constexpr const char* kActionOldest = "terminal.oldest";
inline constexpr const char* kActionNewest = "terminal.newest";

/// The state a same-shape reload keeps: the line the weaver is half-way through typing, and only
/// its text -- the new image places the caret at the end, where the completer needs it. A reload
/// keeps neither a recall nor a scrolled view: it keeps the line as it stood before the recall, and
/// the view follows the newest output again. The line is kept, not dropped as Info's and the
/// Builder's drafts are, for the effort it holds and its inertness until an explicit submit
/// (agents/decisions/the-terminal-is-a-participant.md); a kept line naming a role reaches whoever
/// holds it at submit. The transcript and the completion are re-said or re-asked, never kept.
struct TerminalPaneState {
    std::string line; ///< what the weaver had typed and not yet submitted
    ZEN_SHAPE(TerminalPaneState, 1, ZEN_FIELD(line));
};

} // namespace zengine::terminal_pane

#endif // ZENGINE_TERMINAL_PANE_VOCABULARY_HPP
