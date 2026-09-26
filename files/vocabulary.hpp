// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_FILES_VOCABULARY_HPP
#define ZENGINE_FILES_VOCABULARY_HPP

// The Files tool's durable names: the office it holds, the one pane it offers, the action ids
// its keys answer to, and the state a same-shape reload keeps. A `PaneRef` is what a saved
// setup names, so `zengine.files/project-files` is a promise to a maker's file that one
// constant keeps for the host, the suite and the weave. The pane key stays `project-files`, so
// a desk saved with `zengine.workshop/project-files` converts at load by moving the office
// only; the action ids are the ones makers' keymaps already name.
// Files law: agents/workshop/files.md

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
/// Bounded by Workshop's admission law before a byte is retained: a name at 32 bytes, a
/// summary at 64.
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

// ---- The chooser and the authoring prompt, as the pane's own modes ------------------------
//
// A weave has one room and no context of Workshop's, so the chooser and the authoring prompt
// are a list and a line inside the pane's room, with actions in the pane's own namespace. Each
// mode's Return is an id of its own, because an id is one operation: shared, a Return resolved
// in one mode was acted on in the next. `files.up`, `files.down` and `files.cancel` stay shared.
inline constexpr const char* kActionChoose = "files.choose";            ///< author this candidate
inline constexpr const char* kActionCommitField = "files.commit-field"; ///< commit this field
inline constexpr const char* kActionCancel = "files.cancel";            ///< out of a mode, whole

/// WRITE THE RECIPE THE AUTHORING FIELDS NOW HOLD -- the whole draft, from whichever field the
/// maker is standing on. It is NOT `files.commit-field`, which commits one field and steps to
/// the next: a maker who went back to fix field 1 and pressed Return there would otherwise
/// have to walk the remaining fields again to reach the write. Declared only while the line is
/// open, and refused while a required field is still empty.
inline constexpr const char* kActionWriteRecipe = "files.write-recipe";

/// Keep this field and stand on the next, and nothing else, from the last field: not
/// `files.commit-field`, whose Return writes the recipe from the last field. A control drawn
/// `(next field)` on the last field says the operation does not apply, so a press on it must
/// not write the recipe the adjacent `[write the recipe]` control is for; it refuses in words.
/// No default key (`kUnknown`, WL-KEY-13): Return already steps.
inline constexpr const char* kActionNextField = "files.next-field";

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
inline constexpr const char* kMenuNextField = "files.menu.next-field";
inline constexpr const char* kMenuWriteRecipe = "files.menu.write-recipe";
inline constexpr const char* kMenuCancel = "files.menu.cancel";
inline constexpr const char* kMenuManage = "files.menu.manage";

/// The row id for "type the field at `which`": `files.menu.edit-field:2`. The authoring menu
/// offers a row per field, and `PaneMenuAnswered` echoes one subject for the whole menu (the
/// candidate), so each row's target rides in its id, which the presenter echoes back unread;
/// no other id this pane writes carries a colon.
inline std::string menu_edit_field(std::size_t which) {
    return std::string(kMenuEditField) + ":" + std::to_string(which);
}

/// IS THIS A "TYPE THE FIELD" ROW, AND WHICH FIELD? False for every other id, including the
/// bare prefix: a row id this pane never wrote names no field.
inline bool is_menu_edit_field(const std::string& id, std::size_t* which) {
    const std::string head = std::string(kMenuEditField) + ":";
    if (id.size() <= head.size() || id.compare(0, head.size(), head) != 0) {
        return false;
    }
    std::size_t at = 0;
    for (std::size_t i = head.size(); i < id.size(); ++i) {
        if (id[i] < '0' || id[i] > '9') {
            return false;
        }
        at = at * 10 + static_cast<std::size_t>(id[i] - '0');
    }
    if (which != nullptr) {
        *which = at;
    }
    return true;
}

/// The state a same-shape reload keeps: what a maker is browsing. The listing is re-enumerated
/// at every room grant (WL-FILES-12) and the marks are a durable file, so neither is here;
/// `cursor` is bounded at use, never at write. No mode rides: a half-typed recipe is work in
/// flight a reload is entitled to drop.
struct FilesState {
    std::string current_dir; ///< the one absolute location this browser is showing
    std::int64_t cursor = 0; ///< which row the maker is on
    ZEN_SHAPE(FilesState, 1, ZEN_FIELD(current_dir), ZEN_FIELD(cursor));
};

} // namespace zengine::files

#endif // ZENGINE_FILES_VOCABULARY_HPP
