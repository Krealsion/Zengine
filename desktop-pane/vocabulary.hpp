// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESKTOP_PANE_VOCABULARY_HPP
#define ZENGINE_DESKTOP_PANE_VOCABULARY_HPP

// The Desktop's durable names: the office it holds, the panes it offers, the ids its declared
// actions answer to, and the state a same-shape reload keeps. It owns the behaviour an
// application supplies by default, holds no authority the Weaver did not give it, and is an
// ordinary weave: built by a recipe, loaded by a plan row, reachable from Edit Code and
// replaceable in place. A Workshop whose desktop refused to load has no application defaults.
// Workshop law: agents/workshop/desktop.md

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::desktop_pane {

/// THE OFFICE THIS WEAVE HOLDS. It is spelled in the host too
/// (`workshop/desktop_seam_vocabulary.hpp`'s `kDesktopRole`), because the host must know which
/// role to ask for a declared row -- and a suite pins the two spellings against each other.
inline constexpr const char* kDesktopRole = "zengine.desktop";

/// The Pane Manager: every pane this Workshop knows about, whether it is open, and whether
/// anything offers it -- Return opens or focuses a row, `x` closes it, and `n` makes a pane of the
/// maker's own through the Pane Creator (`s` saves it, `ctrl+d` puts it back). A pane, holding the
/// keys only while a maker has pressed into it. Its durable key is `launcher`, the name the first
/// desks wrote; a desk naming the host's old `pane-editor` is read as it.
inline constexpr const char* kLauncherPane = "launcher";
inline constexpr const char* kLauncherName = "Pane Manager";
inline constexpr const char* kLauncherSummary = "open, focus or close a pane, and make one";

/// THE SECOND PANE: every key in force, where it answers, and how a maker moves one. It replaces
/// the host's key-list overlay, and it keeps no catalog: every row is the host's `KeymapShown`.
inline constexpr const char* kHotkeysPane = "hotkeys";
inline constexpr const char* kHotkeysName = "Hotkeys";
inline constexpr const char* kHotkeysSummary = "every key in force, and how to move one";

/// THE LIBRARY STEM A HOST BOOTS. Not part of any protocol and not durable the way the office
/// is -- it is a file name, here because the host's boot plan and the suite's loader must agree.
inline constexpr const char* kDesktopStem = "zengine-desktop-pane";

// ---- The APPLICATION actions this weave declares (`AppActions`) ---------------------------

/// OPEN OR FOCUS THE TERMINAL. (*) `Ctrl+t` IS BACK, and it is back as something different from
/// what it was: the retired `workshop.terminal` was a GLOBAL row in the host's own catalog that
/// opened an overlay the host compiled. This is an application row a participating weave
/// declares, pointed at an ordinary pane through the host's launch door -- so a maker can move
/// it, disable it, or replace the weave that declares it.
inline constexpr const char* kActionTerminal = "desktop.terminal";

/// OPEN OR FOCUS THE LAUNCHER. The discoverable default the picker's `p` never was: `p` was a
/// bare letter in one mode, so it did nothing while a maker's hands were in a pane.
inline constexpr const char* kActionPanes = "desktop.panes";

/// PUT THE MAKER'S SELECTION DOWN. The host's old hard-coded last word for Escape, declared:
/// same gesture, same position in the chain, owned by a weave a maker can replace -- and
/// disabled outright by a maker who writes `desktop.deselect = none` in their keymap file.
inline constexpr const char* kActionDeselect = "desktop.deselect";

/// OPEN OR FOCUS THE HOTKEYS PANE. `Ctrl+k`, the key the host's own overlay answered to, so the
/// hand goes where it went; `workshop.hotkeys` in a maker's file is read as this id.
inline constexpr const char* kActionHotkeys = "desktop.hotkeys";

// ---- The actions its own pane declares (`PaneActions`) -------------------------------------

inline constexpr const char* kActionUp = "launcher.up";       ///< the row cursor, up
inline constexpr const char* kActionDown = "launcher.down";   ///< ...and down
inline constexpr const char* kActionLaunch = "launcher.open"; ///< launch the row under it
/// TAKE THE ROW UNDER IT OFF THE DESK -- participation, never the provider (`PaneCloseRequested`).
inline constexpr const char* kActionClose = "launcher.close";

inline constexpr const char* kActionKeysUp = "hotkeys.up";       ///< scroll the list up a row
inline constexpr const char* kActionKeysDown = "hotkeys.down";   ///< ...and down
inline constexpr const char* kActionKeysTop = "hotkeys.top";     ///< ...to its first row
inline constexpr const char* kActionKeysBottom = "hotkeys.bottom"; ///< ...and its last
/// THE MENU KEY, in both panes: the same rows a right press on the marked row offers.
inline constexpr const char* kActionMenu = "launcher.menu";
inline constexpr const char* kActionKeysMenu = "hotkeys.menu";
/// ...AND THE TWO KEYS THE HOTKEYS PANE DECLARES WHILE IT IS CAPTURING A KEY OR TAKING A SPELLING:
/// Escape cancels; Return commits a typed spelling. Nothing else is declared then, so every other
/// key reaches the pane as a key (a capture) or as text (a spelling).
inline constexpr const char* kActionKeysCancel = "hotkeys.cancel";
inline constexpr const char* kActionKeysCommit = "hotkeys.commit";

/// THE ROWS THE PANE MANAGER OFFERS ON A ROW'S MENU, and the ones the Hotkeys pane offers on a
/// binding's -- ids in this weave's own namespace, what a `PaneMenuAnswered` carries back.
inline constexpr const char* kMenuManage = "launcher.manage";
inline constexpr const char* kMenuInspect = "launcher.inspect";
inline constexpr const char* kMenuOpen = "launcher.menu-open";
inline constexpr const char* kMenuFocus = "launcher.menu-focus";
inline constexpr const char* kMenuClose = "launcher.menu-close";
inline constexpr const char* kMenuModifyPress = "hotkeys.modify-press";
inline constexpr const char* kMenuModifyType = "hotkeys.modify-type";
inline constexpr const char* kMenuAddPress = "hotkeys.add-press";
inline constexpr const char* kMenuAddType = "hotkeys.add-type";
inline constexpr const char* kMenuRemove = "hotkeys.remove";
inline constexpr const char* kMenuDisable = "hotkeys.disable";
inline constexpr const char* kMenuReset = "hotkeys.reset";

/// THE INSPECTOR'S OFFICE, for the Pane Manager's `inspect` row -- an application choice, spelled
/// in the weave a maker replaces, as the Terminal's office is.
inline constexpr const char* kInfoRole = "zengine.info";

/// The state a same-shape reload keeps: the maker's position, and nothing else -- the inventory
/// is the host's reading, asked for again by every new image. The cursor is an identity (two
/// durable keys), and `cursor` only where the marker sits; Return and `x` act on the identity. A
/// chosen pane that left the list keeps its keys, so a reloaded image still knows the choice is
/// lost; both keys empty means nothing was ever chosen.
struct DesktopState {
    std::int64_t cursor = 0;   ///< which inventory row the marker is on
    std::string cursor_office; ///< the pane chosen, by its two durable keys -- kept when it leaves
    std::string cursor_pane;   ///< the list; both empty until the first row is held
    ZEN_SHAPE(DesktopState, 2, ZEN_FIELD(cursor), ZEN_FIELD(cursor_office),
              ZEN_FIELD(cursor_pane));
};

} // namespace zengine::desktop_pane

#endif // ZENGINE_DESKTOP_PANE_VOCABULARY_HPP
