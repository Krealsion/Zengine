// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_PANE_VOCABULARY_HPP
#define ZENGINE_BUILDER_PANE_VOCABULARY_HPP

// The Builder pane's DURABLE NAMES -- the office it holds, the one pane it offers, the
// action ids its keys answer to, and the state a same-shape reload keeps.
//
// WHY THE OFFICE IS NOT `zengine.builder`. That role is the TOOL's
// (`builder::kBuilderRole`): the participant that holds the recipe views, orders builds
// through the runner and publishes `BuildStatus` and `RecipeCatalog`. This is the pane that
// PRESENTS it, and the two are separate participants for the reason `agents/realization.md`
// BLD-0 states -- a presentation owns no facts. Giving them one office would make
// `send_to_role(zengine.builder, BuildRequested{...})` a message this pane sent to itself.
//
// WHY THE OFFICE AND THE PANE KEY ARE HERE. A `PaneRef` is what a saved setup names, so
// `zengine.builder-pane/builder` is a promise to a maker's file: it survives this build,
// this incarnation and this load order. A constant a host, a suite and the weave all read is
// what keeps three copies of that promise from drifting into two -- `files/vocabulary.hpp`
// and `introspection/vocabulary.hpp` give the same reason for the same choice.
//
// WHY THE PANE KEY IS STILL `builder`. The built-in this tool replaces was
// `zengine.workshop/builder`, and a saved setup naming it is converted to
// `zengine.builder-pane/builder` at load (`workshop/pane_migration.hpp`): the OFFICE moves,
// the pane key does not. Keeping the key is what lets one conversion move a maker's desk
// without touching where on it the pane sits.
//
// WHY THE ACTION IDS ARE `builder.build`, `builder.frontier`, ... A pane declares its
// actions through `PaneActions` (workshop/pane_vocabulary.hpp) and a maker's keymap file
// names them to move them. These are the SAME ids the built-in's command-mode rows carried
// (`workshop/keymap.hpp` `kActionCatalog`, before this migration), so every maker's authored
// `builder.build` keeps working. They were WORKSHOP ids and are the PANE's now: legal
// exactly because the host's rows left in the same commit, and `join_pane_rows` would refuse
// them for as long as both existed (WL-KEY-06, WL-KEY-08).
//
// ⚠ WHAT DID NOT SURVIVE, AND IT IS ONE THING. `authoring.commit` and `authoring.cancel`
// were rows of a WORKSHOP KEYBOARD CONTEXT (`KeyContext::kAuthoring`) that this pane's role
// line replaces, and a context is not a pane: a maker who bound one was binding a mode of the
// host. They retire with it, and `builder.commit`/`builder.cancel` are new ids in this pane's
// own namespace rather than a rename of them. The keymap file keeps such a binding as a
// preserved unknown row and says so once (WL-KEY-08), which is how a maker learns rather than
// finding a dead key. Files reached the same answer about `recipe.*` and `authoring.*` first.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::builder_pane {

/// THE OFFICE THIS PANE HOLDS -- the durable half of its `PaneRef`, and the only address
/// anything reaches it by. A ROLE, so Workshop's `PaneRoom` finds whoever holds it rather
/// than the incarnation that offered first: a reloaded provider is a different `WeaveId` and
/// the same office.
inline constexpr const char* kBuilderPaneRole = "zengine.builder-pane";

/// THE PANE KEY, in this office's namespace. The built-in it replaces carried this same key
/// under `zengine.workshop`; the conversion moves the office and keeps the key.
inline constexpr const char* kBuilderPane = "builder";

/// THE TWO LINES A MAKER READS IN THE PICKER, and what Workshop's pane header says after the
/// office. Bounded by Workshop's admission law before a byte is retained -- a name at 32
/// bytes, a summary at 64 -- and written short deliberately, inside the ten cells the
/// picker's name column actually shows. They are the built-in's own two lines, unchanged.
inline constexpr const char* kBuilderPaneName = "Builder";
inline constexpr const char* kBuilderPaneSummary = "build a chosen recipe";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable the way the
/// office is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kBuilderPaneStem = "zengine-builder-pane";

// ---- THE ACTIONS THE PANE DECLARES (`PaneActions`, workshop/pane_vocabulary.hpp) ---------
//
// The ids a maker's keymap file names to move them, so they live here for the pane key's
// reason and carry the built-in's own spellings unchanged. What each one DOES is the weave's
// (pane.cpp); what KEY requests it is Workshop's effective keymap, and the weave is told the
// id, never the key.

inline constexpr const char* kActionBuild = "builder.build";
inline constexpr const char* kActionBuildRealize = "builder.build-realize";
inline constexpr const char* kActionPromote = "builder.promote";
inline constexpr const char* kActionRevert = "builder.revert";
inline constexpr const char* kActionLoadIt = "builder.load";
inline constexpr const char* kActionRecipeNext = "builder.recipe";
inline constexpr const char* kActionRecipeBack = "builder.recipe-back";
inline constexpr const char* kActionFrontier = "builder.frontier";
inline constexpr const char* kActionEditSource = "builder.edit-source";

// ---- The role line, as the pane's own mode ----------------------------------------------
//
// The built-in opened Workshop's `AuthoringPrompt` -- a host modal in a keyboard context of
// Workshop's own -- for the one line a maker types a plan row's role into. A weave has one
// room and no context of Workshop's, so it becomes a line INSIDE the pane's own room (the
// Files pattern), and its two gestures become two of this pane's declared rows.
//
// ⚠ RETURN IS FREE HERE, WHICH IS WHY THERE IS A SEPARATE COMMIT ID. Files could not add
// one: `files.open` already answered Return while browsing, a pane's rows join into ONE map
// under its runtime handle, and the collision law refuses a second row on a gesture already
// taken (`join_pane_rows`). This pane's nine browsing rows spend `b B P R o c C f e` and
// nothing else, so Return and Escape are unclaimed and the mode can have rows of its own --
// declared only while the line is open, so that nothing is bound to a gesture that means
// nothing (WL-FILES-16's rule, one pane over).

inline constexpr const char* kActionCommit = "builder.commit"; ///< write the role line's row
inline constexpr const char* kActionCancel = "builder.cancel"; ///< abandon the role line

/// THE STATE A SAME-SHAPE RELOAD KEEPS (RELOAD-1).
///
/// TWO FIELDS, AND THEY ARE THE ONLY TWO THAT ARE THE MAKER'S. What this pane is TOLD --
/// the tool's status, its catalog, the project's frontier -- is somebody else's facts and is
/// re-asked at the next room grant, so keeping it would make this pane a second owner of
/// them (`agents/decisions/a-presentation-owns-no-facts.md`, and the reason the built-in's
/// `BuildStatus` copy died with its panel, WL-PROJ-12). What is genuinely this pane's is
/// which row the maker picked out and whether the next build should be offered to the
/// running project.
///
/// ⚠ `chosen` IS A RECIPE NAME AND NOT AN INDEX. The built-in held an index and followed it
/// by name whenever the catalog moved (WL-PROJ-07); across a reload an index is worse still,
/// because the catalog is re-asked and may come back reordered before the number means
/// anything. Holding the name makes "follow the choice to its new row" the only rule there
/// is, and an empty string the honest absence.
///
/// IT CARRIES NO MODE. The role line is work in flight that a reload is entitled to drop,
/// exactly as the Editor's live draft is: a reload lands with the pane where it was and no
/// half-typed role, which is the honest answer for a mode that was mid-gesture.
///
/// AND IT CARRIES NO `awaiting`. That latch is "I asked and have not been answered" -- a fact
/// about an ask this incarnation made (WL-PROJ-11). A reloaded image made no such ask, so
/// carrying the latch across would make it announce, as news it watched, an answer to
/// somebody else's question.
struct BuilderPaneState {
    std::string chosen; ///< the recipe the maker picked out, by name; empty means none
    bool arm = false;   ///< the next build is offered to the running project when it works
    ZEN_SHAPE(BuilderPaneState, 1, ZEN_FIELD(chosen), ZEN_FIELD(arm));
};

} // namespace zengine::builder_pane

#endif // ZENGINE_BUILDER_PANE_VOCABULARY_HPP
