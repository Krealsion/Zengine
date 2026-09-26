// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_PANE_VOCABULARY_HPP
#define ZENGINE_BUILDER_PANE_VOCABULARY_HPP

// The Builder pane's durable names: the office it holds, the one pane it offers, the action ids
// its keys answer to, and the state a same-shape reload keeps. The office is not the tool's
// `zengine.builder`: the tool owns the facts and this pane presents them, and one office would
// make a `BuildRequested` a message to itself. The pane key stays `builder`, so a desk saved as
// `zengine.workshop/builder` converts at load by moving the office only
// (`workshop/pane_migration.hpp`).
// Builder law: agents/realization.md

// The action ids are the ones makers' keymaps already name, and they are the pane's now
// (WL-KEY-06, WL-KEY-08). The role line keeps `authoring.commit` and `authoring.cancel`, ids of
// a Workshop context whose one asker was this prompt, so a maker's override still applies;
// legal because Workshop declares neither any more (`join_pane_rows`).

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::builder_pane {

/// THE OFFICE THIS PANE HOLDS -- the durable half of its `PaneRef`, and the only address
/// anything reaches it by. A ROLE, so Workshop's `PaneRoom` finds whoever holds it rather
/// than the incarnation that offered first: a reloaded provider is a different `WeaveId` and
/// the same office.
inline constexpr const char* kBuilderPaneRole = "zengine.builder-pane";

/// The pane key, in this office's namespace: a converted desk keeps it.
inline constexpr const char* kBuilderPane = "builder";

/// The two lines a maker reads about this pane (its name in the Pane Manager's list, its
/// summary in Info), also what Workshop's pane header says after the office; bounded by
/// Workshop's admission law (32 and 64 bytes) and written short.
inline constexpr const char* kBuilderPaneName = "Builder";
inline constexpr const char* kBuilderPaneSummary = "build a chosen recipe";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable the way the
/// office is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kBuilderPaneStem = "zengine-builder-pane";

// ---- The actions the pane declares (`PaneActions`, workshop/pane_vocabulary.hpp) ---------
//
// The ids a maker's keymap file names to move them. What each one does is the weave's
// (pane.cpp); which key requests it is Workshop's effective keymap, and the weave is told the
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
/// Read what a build said: the pane becomes a reader bound to one operation's output until it
/// is closed (WL-OUT-04).
inline constexpr const char* kActionOutput = "builder.output";

// ---- The output reader, as the pane's own mode ------------------------------------------
//
// THE ROLE LINE'S ARRANGEMENT, ONE MODE OVER: while the reader is open the pane declares these
// rows and no others, so the arrows scroll and pan instead of meaning nothing, and every build
// verb is out of reach until Escape closes it -- a reader cannot build by a slip of the hand.

inline constexpr const char* kActionOutputUp = "builder.output-up";
inline constexpr const char* kActionOutputDown = "builder.output-down";
inline constexpr const char* kActionOutputFirst = "builder.output-first";
inline constexpr const char* kActionOutputLast = "builder.output-last";
inline constexpr const char* kActionOutputLeft = "builder.output-left";
inline constexpr const char* kActionOutputRight = "builder.output-right";
inline constexpr const char* kActionOutputOlder = "builder.output-older";
inline constexpr const char* kActionOutputNewer = "builder.output-newer";
inline constexpr const char* kActionOutputClose = "builder.output-close";

// ---- The role line, as the pane's own mode ----------------------------------------------
//
// The one line a maker types a plan row's role into, inside the pane's own room (a weave has no
// keyboard context of Workshop's). Its rows are declared only while the line is open, so
// nothing is bound to a gesture that means nothing there (WL-FILES-16).

inline constexpr const char* kActionCommit = "authoring.commit"; ///< write the role line's row
inline constexpr const char* kActionCancel = "authoring.cancel"; ///< abandon the role line

// ---- The two halves of `builder.build-realize`, as operations of their own ---------------
//
// One id with two meanings (a button when an artifact waits, a toggle elsewhere) is a key's
// bargain, not a button's: a hand aiming at `[load built rocket.dll]` must get that, whatever
// the state became meanwhile. So each half has its own id, refusing in words where it does not
// apply, with no default key (`kUnknown`, WL-KEY-13); the shipped key keeps both meanings.
inline constexpr const char* kActionArm = "builder.arm";
inline constexpr const char* kActionLoadBuilt = "builder.load-built";

// ---- The recipe list, as the pane's own mode --------------------------------------------
//
// `builder.recipe` walks the catalog a row at a time, which is no way to choose with a hand; the
// list shows the catalog on rows. Its cursor is not the choice: `builder.recipe-choose` makes
// the row the maker's pick, and `builder.recipes-close` leaves the choice as it was, so
// looking at a recipe never arms the next build against it.
inline constexpr const char* kActionRecipes = "builder.recipes";
inline constexpr const char* kActionRecipesUp = "builder.recipes-up";
inline constexpr const char* kActionRecipesDown = "builder.recipes-down";
inline constexpr const char* kActionRecipeChoose = "builder.recipe-choose";
inline constexpr const char* kActionRecipesClose = "builder.recipes-close";

/// THE PANE'S OWN MENU ON WHAT IS SELECTED -- the keyboard's way to the rows a right press
/// offers, so the mouse and the keyboard reach one list of operations (WL-CTX-09).
inline constexpr const char* kActionMenu = "builder.menu";

// ---- The ids the pane's own menu rows carry ---------------------------------------------
//
// NOT ACTION IDS. A menu row's id crosses to the presenter and comes back in
// `PaneMenuAnswered`; it is never resolved against a keymap and never declared through
// `PaneActions`. They live here beside the action ids so one file says every name this pane
// answers to, and they are spelled apart from the action namespace on purpose.

inline constexpr const char* kMenuBuild = "builder.menu.build";
inline constexpr const char* kMenuArm = "builder.menu.arm";
inline constexpr const char* kMenuLoadBuilt = "builder.menu.load-built";
inline constexpr const char* kMenuAddToPlan = "builder.menu.add-to-plan";
inline constexpr const char* kMenuFrontier = "builder.menu.frontier";
inline constexpr const char* kMenuPromote = "builder.menu.promote";
inline constexpr const char* kMenuRevert = "builder.menu.revert";
inline constexpr const char* kMenuEditSource = "builder.menu.edit-source";
inline constexpr const char* kMenuOutput = "builder.menu.output";
inline constexpr const char* kMenuRecipes = "builder.menu.recipes";
inline constexpr const char* kMenuChoose = "builder.menu.choose";
inline constexpr const char* kMenuClose = "builder.menu.close";
inline constexpr const char* kMenuCommit = "builder.menu.commit";
inline constexpr const char* kMenuCancel = "builder.menu.cancel";
inline constexpr const char* kMenuManage = "builder.menu.manage";

// ---- The output reader's own rows -------------------------------------------------------
//
// A menu must carry its mode's controls, or a narrow strip's `+N in menu` breaks its promise:
// each reader operation has a row id of its own, and the pane's completeness case walks every
// mode's strip against every mode's menu.
inline constexpr const char* kMenuOutputUp = "builder.menu.output-up";
inline constexpr const char* kMenuOutputDown = "builder.menu.output-down";
inline constexpr const char* kMenuOutputFirst = "builder.menu.output-first";
inline constexpr const char* kMenuOutputLast = "builder.menu.output-last";
inline constexpr const char* kMenuOutputLeft = "builder.menu.output-left";
inline constexpr const char* kMenuOutputRight = "builder.menu.output-right";
inline constexpr const char* kMenuOutputOlder = "builder.menu.output-older";
inline constexpr const char* kMenuOutputNewer = "builder.menu.output-newer";

/// The state a same-shape reload keeps: the two fields that are the maker's -- which recipe they
/// picked out, and whether the next build is offered to the running project. What this pane is
/// told (the tool's status and catalog, the frontier) is re-asked at the next room grant, since
/// a presentation owns no facts (`agents/decisions/a-presentation-owns-no-facts.md`). `chosen`
/// is a name, not an index, because a re-asked catalog may come back reordered. No mode rides,
/// and no `awaiting`: a reloaded image asked nothing, so it must not announce an answer as news.
struct BuilderPaneState {
    std::string chosen; ///< the recipe the maker picked out, by name; empty means none
    bool arm = false;   ///< the next build is offered to the running project when it works
    ZEN_SHAPE(BuilderPaneState, 1, ZEN_FIELD(chosen), ZEN_FIELD(arm));
};

} // namespace zengine::builder_pane

#endif // ZENGINE_BUILDER_PANE_VOCABULARY_HPP
