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

/// THE TWO LINES A MAKER READS IN THE PICKER, and what Workshop's pane header says after
/// the office. Bounded by Workshop's admission law before a byte is retained -- a name at
/// 32 bytes, a summary at 64 -- and written short deliberately, inside the ten cells the
/// picker's name column actually shows.
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

// ⚠ THERE IS NO SEPARATE `files.choose`. The built-in spelled the chooser's commit and the
// authoring line's commit as rows in their OWN keyboard contexts (`kRecipeChooser`,
// `kAuthoring`), all three answering to Return; a PANE has ONE context -- its own runtime
// handle -- so two of its rows cannot answer to one gesture and the collision law refuses
// the declaration whole (`join_pane_rows`). So Return is `files.open`, and what it means is
// the pane's business: enter or edit while browsing, commit while a mode is open. A maker's
// keymap moves the gesture once and it moves in every mode, which is the honest reading of
// "one pane, one key map" and is what a maker sees anyway.  ///< commit a chooser row / an authoring field
inline constexpr const char* kActionCancel = "files.cancel";  ///< back out of a mode whole

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
