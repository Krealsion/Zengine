// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_FILES_VOCABULARY_HPP
#define ZENGINE_FILES_VOCABULARY_HPP

// The Files tool's DURABLE NAMES -- the office it holds, the one pane it offers, the
// action ids its keys answer to, and the state a same-shape reload keeps.
//
// WHY THE OFFICE AND THE PANE KEY ARE HERE. A `PaneRef` is what a saved setup names, so
// `zengine.files/project-files` is a promise to a maker's file: it survives this build,
// this incarnation and this load order. A constant a host, a suite and the weave all read
// is what keeps three copies of that promise from drifting into two -- the reason
// `introspection/vocabulary.hpp` gives for the same choice.
//
// WHY THE PANE KEY IS STILL `project-files`. The built-in this tool replaces was
// `zengine.workshop/project-files`, and a saved setup naming it is converted to
// `zengine.files/project-files` at load (a WL-MIG step): the OFFICE moves, the pane key
// does not. Keeping the key is what lets one conversion move a maker's desk without
// touching where on it the pane sits.
//
// WHY THE ACTION IDS ARE `files.up`, `files.open`, ... A pane declares its actions through
// `PaneActions` (workshop/pane_vocabulary.hpp) and a maker's keymap file names them to move
// them. These are the SAME ids the built-in's `kFiles` rows carried, so every maker's
// authored `files.up` keeps working across the migration. A pane that renamed one would
// break every override that moved it.

#include <zen/weave/shape.hpp>

#include <string>

namespace zengine::files {

/// THE OFFICE THIS TOOL HOLDS -- the durable half of its `PaneRef`, and the only address
/// anything reaches it by. A ROLE, so Workshop's `PaneRoom` finds whoever holds it rather
/// than the incarnation that offered first: a reloaded provider is a different `WeaveId`
/// and the same office.
inline constexpr const char* kFilesRole = "zengine.files";

/// THE PANE KEY, in this office's namespace. The built-in it replaces carried this same
/// key under `zengine.workshop`; the conversion moves the office and keeps the key.
inline constexpr const char* kProjectFilesPane = "project-files";

/// THE TWO LINES A MAKER READS ABOUT THIS PANE -- its name in the Pane Manager's list, its
/// summary in Info -- and what Workshop's pane header says after the office.
/// Bounded by Workshop's admission law before a byte is retained -- a name at 32 bytes, a
/// summary at 64 -- and written short deliberately, to the ten cells the retired picker's
/// name column showed.
inline constexpr const char* kProjectFilesName = "Files";
inline constexpr const char* kProjectFilesSummary = "browse and open files";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable the way the
/// office is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kFilesStem = "zengine-files";

// ---- THE ACTIONS THE PANE DECLARES (`PaneActions`, workshop/pane_vocabulary.hpp) --------
//
// The ids a maker's keymap file names to move them, so they live here for the pane key's
// reason and carry the built-in's own spellings unchanged. What each one DOES is the
// weave's (files.cpp); what KEY requests it is Workshop's effective keymap, and the weave
// is told the id, never the key.

inline constexpr const char* kActionUp = "files.up";
inline constexpr const char* kActionDown = "files.down";
inline constexpr const char* kActionOpen = "files.open";
inline constexpr const char* kActionParent = "files.parent";
inline constexpr const char* kActionRefresh = "files.refresh";
inline constexpr const char* kActionUseRecipes = "files.use-recipes";
inline constexpr const char* kActionMark = "files.mark";
inline constexpr const char* kActionNextMark = "files.next-mark";
inline constexpr const char* kActionPreviousMark = "files.previous-mark";
inline constexpr const char* kActionPickBuildable = "files.pick-buildable";

// ---- The chooser and the authoring prompt, as the pane's own modes ----------------------
//
// The built-in drew these as popups over the desk in Workshop's own two contexts
// (`kRecipeChooser`, `kAuthoring`). A weave has one room and no context of Workshop's, so
// they become a list and a line INSIDE the pane's own room (the Powers pattern). Their
// navigation and commit are the pane's declared actions too, in the pane's own namespace --
// deliberately NOT the built-in's `recipe.*`/`authoring.*` ids, because those were Workshop
// contexts a maker's keymap could name and this pane's modes are not: a maker who bound
// `recipe.choose` was binding a Workshop context that no longer exists.
//
// ⚠ EACH MODE'S RETURN IS AN ID OF ITS OWN, BECAUSE AN ID IS ONE OPERATION. The collision law
// judges the declaration in force (`join_pane_rows`), and the pane replaces that declaration
// whenever its mode changes (files.cpp `declare`), so the three rows on Return are never
// declared together and never meet. One id for all three was not forced by that law, and it
// let a Return Workshop resolved against one mode be acted on in the next, where it meant
// something else: Escape and Return in one poll cancelled the line, then opened the file under
// the cursor. So `files.open` is browsing's enter-or-edit and nothing more, the chooser's row
// is `files.choose`, the line's field is `files.commit-field`, and a maker's keymap moves each
// alone. `files.up`, `files.down` and `files.cancel` stay shared because each means one thing
// in every mode that declares it: the cursor of the list shown, and out of the mode open.
inline constexpr const char* kActionChoose = "files.choose";            ///< author this candidate
inline constexpr const char* kActionCommitField = "files.commit-field"; ///< commit this field
inline constexpr const char* kActionCancel = "files.cancel";            ///< out of a mode, whole

/// WRITE THE RECIPE THE AUTHORING FIELDS NOW HOLD -- the whole draft, from whichever field the
/// maker is standing on. It is NOT `files.commit-field`, which commits one field and steps to
/// the next: a maker who went back to fix field 1 and pressed Return there would otherwise
/// have to walk the remaining fields again to reach the write. Declared only while the line is
/// open, and refused while a required field is still empty.
inline constexpr const char* kActionWriteRecipe = "files.write-recipe";

/// THE PANE'S OWN MENU ON WHAT IS SELECTED -- the keyboard's way to the rows a right press
/// offers, so the mouse and the keyboard reach one list of operations. Its subject is the
/// selected entry while browsing, the selected candidate in the chooser, and the field the
/// line is standing on; it is the pane's request and the presenter's presentation (WL-CTX-09).
inline constexpr const char* kActionMenu = "files.menu";

// ---- The ids the pane's own menu rows carry ---------------------------------------------
//
// NOT ACTION IDS. A menu row's id crosses to the presenter and comes back in `PaneMenuAnswered`;
// it is never resolved against a keymap and never declared through `PaneActions`. They live
// here beside the action ids so one file says every name this pane answers to, and they are
// spelled apart from the action namespace on purpose -- a maker's keymap cannot name them.

inline constexpr const char* kMenuOpen = "files.menu.open";
inline constexpr const char* kMenuUseRecipes = "files.menu.use-recipes";
inline constexpr const char* kMenuPickBuildable = "files.menu.pick-buildable";
inline constexpr const char* kMenuMark = "files.menu.mark";
inline constexpr const char* kMenuParent = "files.menu.parent";
inline constexpr const char* kMenuRefresh = "files.menu.refresh";
inline constexpr const char* kMenuNextMark = "files.menu.next-mark";
inline constexpr const char* kMenuPreviousMark = "files.menu.previous-mark";
inline constexpr const char* kMenuChoose = "files.menu.choose";
inline constexpr const char* kMenuEditField = "files.menu.edit-field";
inline constexpr const char* kMenuWriteRecipe = "files.menu.write-recipe";
inline constexpr const char* kMenuCancel = "files.menu.cancel";
inline constexpr const char* kMenuManage = "files.menu.manage";

/// THE STATE A SAME-SHAPE RELOAD KEEPS (RELOAD-1). What a maker is browsing survives a
/// reload of this weave's own image; the listing is re-enumerated at every room grant
/// (WL-FILES-12) and is not here, and the marks are a durable FILE this pane owns, not
/// state. `cursor` is bounded at use, never at write, the built-in's own rule.
///
/// IT CARRIES NO MODE. The chooser and the authoring prompt are work in flight that a
/// reload is entitled to drop, exactly as the Editor's live draft is: a reload lands with
/// the browser where it was and no half-typed recipe, which is the honest answer for a
/// mode that was mid-gesture.
struct FilesState {
    std::string current_dir; ///< the one absolute location this browser is showing
    std::int64_t cursor = 0; ///< which row the maker is on
    ZEN_SHAPE(FilesState, 1, ZEN_FIELD(current_dir), ZEN_FIELD(cursor));
};

} // namespace zengine::files

#endif // ZENGINE_FILES_VOCABULARY_HPP
