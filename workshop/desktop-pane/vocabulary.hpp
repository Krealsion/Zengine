// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESKTOP_PANE_VOCABULARY_HPP
#define ZENGINE_DESKTOP_PANE_VOCABULARY_HPP

// The Desktop's durable names: the office it holds, the panes it offers, the ids its declared
// actions answer to, and the state a same-shape reload keeps. It owns the behaviour an
// application supplies by default, holds no authority the host did not grant it, and is an
// ordinary weave: built by a recipe, loaded by a plan row, reachable from Edit Code and
// replaceable in place. A Workshop whose desktop refused to load has no application defaults.
// Workshop law: agents/workshop/desktop.md

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::desktop_pane {

/// THE OFFICE THIS WEAVE HOLDS. It is spelled in the host too
/// (`workshop/desktop_seam_vocabulary.hpp`'s `kDesktopRole`), because the host must know which
/// role to ask for a declared row -- and a suite pins the two spellings against each other.
inline constexpr const char* kDesktopRole = "zengine.desktop";

/// The Pane Manager: every pane this Workshop knows about, whether it is open, and whether
/// anything offers it -- Return opens or focuses a row, `x` closes it, and `n` shows the View
/// Builder, where a weaver makes a pane of their own. A pane, holding the keys only while a weaver
/// has pressed into it. Its durable key is `launcher`, the name the first desks wrote; a desk
/// naming the host's old `pane-editor` is read as it.
inline constexpr const char* kLauncherPane = "launcher";
inline constexpr const char* kLauncherName = "Pane Manager";
inline constexpr const char* kLauncherSummary = "show, focus or hide a pane, and make one";

/// THE SECOND PANE: every key in force, where it answers, and how a weaver moves one. It replaces
/// the host's key-list overlay, and it keeps no catalog: every row is the host's `KeymapShown`.
inline constexpr const char* kHotkeysPane = "hotkeys";
inline constexpr const char* kHotkeysName = "Hotkeys";
inline constexpr const char* kHotkeysSummary = "every key in force, and how to move one";

/// THE ONE SETTING THE HOTKEYS PANE DECLARES (`v2::PaneSettingsDeclared`,
/// workshop/pane_settings.hpp): whether its footer's help lines -- the keymap file and how a key is
/// written there -- show in a layout, on unless the layout keeps it off.
inline constexpr const char* kSettingHelp = "help";

/// THE THIRD PANE: the manual of the pane whose `?` a weaver pressed -- `About`, `Use`, then each
/// command with the key it answers to now. It asks that pane for its words when it is shown and
/// keeps them only while it is open.
inline constexpr const char* kManualPane = "manual";
inline constexpr const char* kManualName = "Manual";
inline constexpr const char* kManualSummary = "the manual of the pane whose ? you pressed";

/// THE LIBRARY STEM A HOST BOOTS. Not part of any protocol and not durable the way the office
/// is -- it is a file name, here because the host's boot plan and the suite's loader must agree.
inline constexpr const char* kDesktopStem = "zengine-desktop-pane";

// ---- The APPLICATION actions this weave declares (`AppActions`) ---------------------------

/// OPEN OR FOCUS THE TERMINAL: an application row a participating weave declares, pointed at
/// an ordinary pane through the host's launch door -- so a weaver can move it, disable it, or
/// replace the weave that declares it.
inline constexpr const char* kActionTerminal = "desktop.terminal";

/// OPEN OR FOCUS THE LAUNCHER, on a chord, so it works while a weaver's hands are in a pane.
inline constexpr const char* kActionPanes = "desktop.panes";

/// PUT THE WEAVER'S SELECTION DOWN: the last word for Escape in the chain, owned by a weave a
/// weaver can replace, and disabled outright by a weaver who writes `desktop.deselect = none` in
/// their keymap file.
inline constexpr const char* kActionDeselect = "desktop.deselect";

/// SHOW OR HIDE THE COMMAND PALETTE, the list of everything a hand can do by name, above every
/// mode: `Ctrl+Space`, which every window says once a POSIX terminal reads its byte 0.
inline constexpr const char* kActionPalette = "desktop.palette";
/// ...AND ON DOUBLE SHIFT, the OS window's second key for it: Shift tapped twice, alone.
inline constexpr const char* kActionPaletteShift = "desktop.palette-shift";

/// OPEN OR FOCUS THE HOTKEYS PANE. `Ctrl+k`, the key the host's own overlay answered to, so the
/// hand goes where it went; `workshop.hotkeys` in a weaver's file is read as this id.
inline constexpr const char* kActionHotkeys = "desktop.hotkeys";

/// SHOW A PANE'S MANUAL: what its `?` asks, and a chord above every mode, asked about the pane
/// holding the keys. Spelled in the host too
/// (`workshop/desktop_seam_vocabulary.hpp`'s `kManualAction`), which draws the `?` while it is
/// declared, and a suite pins the two spellings against each other.
inline constexpr const char* kActionManual = "desktop.manual";

// ---- The actions its own pane declares (`PaneActions`) -------------------------------------

inline constexpr const char* kActionUp = "launcher.up";       ///< the row cursor, up
inline constexpr const char* kActionDown = "launcher.down";   ///< ...and down
inline constexpr const char* kActionLaunch = "launcher.open"; ///< launch the row under it
/// TAKE THE ROW UNDER IT OFF THE DESK -- participation, never the provider (`PaneCloseRequested`).
inline constexpr const char* kActionClose = "launcher.close";
/// SHOW THE VIEW BUILDER, where a pane of the weaver's own is made and saved: the Pane Manager's
/// way to make a pane, through the host's launch door like any tool.
inline constexpr const char* kActionNew = "launcher.new";

inline constexpr const char* kActionKeysUp = "hotkeys.up";       ///< scroll the list up a row
inline constexpr const char* kActionKeysDown = "hotkeys.down";   ///< ...and down
inline constexpr const char* kActionKeysTop = "hotkeys.top";     ///< ...to its first row
inline constexpr const char* kActionKeysBottom = "hotkeys.bottom"; ///< ...and its last
/// THE TABLE'S SECTIONS, a section a pane: open the one the cursor is on, or step into it when it
/// is open; shut it, or from a key step to its header; and shut every section in one act.
inline constexpr const char* kActionKeysOpen = "hotkeys.open";
inline constexpr const char* kActionKeysShut = "hotkeys.shut";
inline constexpr const char* kActionKeysShutAll = "hotkeys.shut-all";
/// THE MENU KEY, in both panes: the same rows a right press on the marked row offers.
inline constexpr const char* kActionMenu = "launcher.menu";
inline constexpr const char* kActionKeysMenu = "hotkeys.menu";
/// ...AND THE TWO KEYS THE HOTKEYS PANE DECLARES WHILE IT IS CAPTURING A KEY OR TAKING A SPELLING:
/// Escape cancels; Return commits a typed spelling. Nothing else is declared then, so every other
/// key reaches the pane as a key (a capture) or as text (a spelling).
inline constexpr const char* kActionKeysCancel = "hotkeys.cancel";
inline constexpr const char* kActionKeysCommit = "hotkeys.commit";

/// ...AND THE MANUAL PANE'S: the lines up and down, its two ends, the copy of what is selected,
/// everything selected, and close.
inline constexpr const char* kActionManualUp = "manual.up";
inline constexpr const char* kActionManualDown = "manual.down";
inline constexpr const char* kActionManualTop = "manual.top";
inline constexpr const char* kActionManualBottom = "manual.bottom";
inline constexpr const char* kActionManualCopy = "manual.copy";
inline constexpr const char* kActionManualAll = "manual.all";
inline constexpr const char* kActionManualClose = "manual.close";
/// THE MANUAL'S TWO PARTS IN A WINDOW: its text, and the bar beside it.
inline constexpr const char* kManualTextPart = "manual.text";
inline constexpr const char* kManualBarPart = "manual.bar";

/// THE MODES EACH PANE DECLARES ITS ROWS IN, by the names the action census and a manual use: the
/// Pane Manager's one list; the Hotkeys pane's table, a key being captured, and a spelling being
/// typed; the Manual's reading.
inline constexpr const char* kLauncherMode = "list";
inline constexpr const char* kKeysTableMode = "table";
inline constexpr const char* kKeysCaptureMode = "capture";
inline constexpr const char* kKeysSpellMode = "spell";
inline constexpr const char* kManualMode = "read";

/// WHAT A PANE MANAGER ROW'S PART IS NAMED BY: this, then the row's pane as `<office>/<pane>`.
inline constexpr const char* kLauncherRowPrefix = "pane:";

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
/// in the weave a weaver replaces, as the Terminal's office is.
inline constexpr const char* kInfoRole = "zengine.info";

/// The state a same-shape reload keeps: the weaver's position, and which of the Hotkeys pane's
/// sections are open -- the inventory and the keymap are the host's readings, asked again by every
/// new image. The cursor is an identity (two durable keys), and `cursor` only where the marker
/// sits; Return and `x` act on the identity. A chosen pane that left the list keeps its keys, so a
/// reloaded image still knows the choice is lost; both keys empty means nothing was ever chosen.
struct DesktopState {
    std::int64_t cursor = 0;   ///< which inventory row the marker is on
    std::string cursor_office; ///< the pane chosen, by its two durable keys -- kept when it leaves
    std::string cursor_pane;   ///< the list; both empty until the first row is held
    std::vector<std::string> hotkeys_open = {}; ///< the Hotkeys pane's open sections, by identity
    ZEN_SHAPE(DesktopState, 3, ZEN_FIELD(cursor), ZEN_FIELD(cursor_office),
              ZEN_FIELD(cursor_pane), ZEN_FIELD(hotkeys_open));
};

} // namespace zengine::desktop_pane

#endif // ZENGINE_DESKTOP_PANE_VOCABULARY_HPP
