// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESKTOP_PANE_VOCABULARY_HPP
#define ZENGINE_DESKTOP_PANE_VOCABULARY_HPP

// The Desktop's DURABLE NAMES -- the office it holds, the one pane it offers, the ids its
// declared actions answer to, and the state a same-shape reload keeps.
//
// WHAT THIS WEAVE IS. It owns the behaviour an application supplies by default: which gestures
// open or focus which tool, what happens to a selection when nothing more specific claims the
// key, what a maker reads in an empty room, and what to say about a tool that is not there. All
// of that used to be compiled into the host -- a global row in `kActionCatalog`, a hard-coded
// line at the end of the key handler, an overlay picker, and a prototype rectangle document.
//
// WHAT IT IS NOT. It is not the host and it is not the fixed root. It holds no authority the
// Weaver did not give it, opens nothing the host's inventory does not already hold, loads no
// artifact, and cannot keep a maker from leaving. A Workshop whose desktop refused to load is a
// Workshop with no application defaults -- which the band, the hotkey view and the baseline
// console all say, and which a maker recovers from by fixing the artifact and launching again.
//
// (!) AND IT IS AN ORDINARY WEAVE. It is built by an ordinary recipe, loaded by an ordinary plan
// row, reachable from Edit Code like any pane, and REPLACEABLE in place through the ordinary
// realization loop: change what it declares or what its floor says, build it, reload it, and
// the application's defaults are the new image's. Nothing about being the desktop makes it
// harder to replace than the Attention pane.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::desktop_pane {

/// THE OFFICE THIS WEAVE HOLDS. It is spelled in the host too
/// (`workshop/desktop_seam_vocabulary.hpp`'s `kDesktopRole`), because the host must know which
/// role to ask for a declared row -- and a suite pins the two spellings against each other.
inline constexpr const char* kDesktopRole = "zengine.desktop";

/// THE PANE MANAGER: every pane this Workshop knows about, whether it is open, and whether anything
/// is offering it -- Return on a row opens or focuses it, `x` closes it, and `n` makes a pane of
/// the maker's own through the Pane Creator (`s` saves it, `ctrl+d` puts it back).
///
/// (*) IT REPLACES THE `p` PICKER AND THE HOST'S OWN PANE MANAGER, and the difference is not
/// cosmetic. The picker was a MODE the host owned: it took the keyboard whole, it toggled
/// participation (pressing a row for an open pane removed it), and nothing could replace it. The
/// host's manager was a built-in that also inspected; inspecting is Info's now. This is a pane:
/// arranged on the desk like any other, holding the keys only while a maker has pressed into it,
/// and what its keys mean is this weave's to change. Its durable key is still `launcher`, the
/// name the first desks that held it wrote; a desk naming the host's `pane-editor` is read as it.
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

/// THE STATE A SAME-SHAPE RELOAD KEEPS (RELOAD-1).
///
/// THE MAKER'S POSITION, AND NOTHING ELSE. The inventory this pane lists is the host's reading,
/// asked for again by every new image; a copy kept across a reload would present a picture
/// derived before this image existed.
///
/// THE CURSOR IS AN IDENTITY, NOT AN INDEX. Rows come and go as providers offer and leave, so
/// the row a maker was standing on is found again by its two durable keys; `cursor` is only
/// where the marker sits while it is found, or where it was when that pane left the list.
/// Return acts on the identity, never on whatever row now has that index.
struct DesktopState {
    std::int64_t cursor = 0;   ///< which inventory row the marker is on
    std::string cursor_office; ///< the pane under it, by its two durable keys; both empty when
    std::string cursor_pane;   ///< the maker has not chosen one since that pane left the list
    ZEN_SHAPE(DesktopState, 2, ZEN_FIELD(cursor), ZEN_FIELD(cursor_office),
              ZEN_FIELD(cursor_pane));
};

} // namespace zengine::desktop_pane

#endif // ZENGINE_DESKTOP_PANE_VOCABULARY_HPP
