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
// ⚠ AND THE ROLE LINE KEEPS `authoring.commit` / `authoring.cancel` -- ids in a namespace this
// pane does not own, deliberately. They were rows of a WORKSHOP KEYBOARD CONTEXT
// (`KeyContext::kAuthoring`) whose one asker was the built-in Builder's role prompt; the prompt
// moved inside this pane's own room, and it is the same one line, the same commit and the same
// cancel. So a maker who authored `authoring.commit` keeps the key they moved, which is the
// whole promise this migration was made to keep -- and the alternative, minting `builder.commit`,
// would have left their row preserved-but-dead for no gain.
//
// IT IS LEGAL BECAUSE THE HOST DECLARES NEITHER ANY MORE. The id law refuses a pane a row in
// WORKSHOP's namespace (`join_pane_rows`), and what that means is the ids Workshop currently
// declares -- these left `kActionCatalog` in the same commit that made this pane. Files could
// not do this with `recipe.*`: those were TWO contexts collapsing into one pane, and Return
// was already taken by a browsing row.

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

/// THE TWO LINES A MAKER READS ABOUT THIS PANE -- its name in the Pane Manager's list, its
/// summary in Info -- and what Workshop's pane header says after the office.
/// Bounded by Workshop's admission law before a byte is retained -- a name at 32 bytes, a
/// summary at 64 -- and written short deliberately, to the ten cells the retired picker's
/// name column showed. They are the built-in's own two lines, unchanged.
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
/// READ WHAT A BUILD SAID: the pane becomes a reader bound to one operation's output until
/// it is closed (WL-OUT-04). New with this action, so no maker's keymap names it yet.
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
// The built-in opened Workshop's `AuthoringPrompt` -- a host modal in a keyboard context of
// Workshop's own -- for the one line a maker types a plan row's role into. A weave has one
// room and no context of Workshop's, so it becomes a line INSIDE the pane's own room (the
// Files pattern), and its two gestures become two of this pane's declared rows.
//
// ⚠ RETURN IS FREE HERE, WHICH IS WHY THE MODE CAN HAVE ROWS AT ALL. Files could not add one:
// `files.open` already answered Return while browsing, a pane's rows join into ONE map under
// its runtime handle, and the collision law refuses a second row on a gesture already taken
// (`join_pane_rows`). This pane's nine browsing rows spend `b B P R o c C f e` and nothing
// else, so Return and Escape are unclaimed -- and the two rows are declared only WHILE the line
// is open, so nothing is bound to a gesture that means nothing (WL-FILES-16's rule, one pane
// over). See the header note above for why they keep the ids they had.

inline constexpr const char* kActionCommit = "authoring.commit"; ///< write the role line's row
inline constexpr const char* kActionCancel = "authoring.cancel"; ///< abandon the role line

// ---- The two halves of `builder.build-realize`, as operations of their own ---------------
//
// ⚠ ONE ID WITH TWO MEANINGS IS A KEY'S BARGAIN, NOT A BUTTON'S. `builder.build-realize` is a
// BUTTON when an artifact is built and waiting and a TOGGLE everywhere else, and for a key
// that is a convenience: a maker presses `Shift+B` and reads the sentence it wrote. A control
// under a hand cannot work that way -- a maker aims at `[load built rocket.dll]` and must get
// that and never an arming of the next build, whatever the state became while the press was in
// flight. So the two halves have ids of their own, each refusing in words when it does not
// apply, and the shipped key keeps both meanings exactly as it had them.
//
// NEITHER CARRIES A DEFAULT KEY (`kUnknown`, WL-KEY-13): they exist so a control and a menu row
// can name one operation each, and a maker who wants a key for one names the id in their keymap.
inline constexpr const char* kActionArm = "builder.arm";
inline constexpr const char* kActionLoadBuilt = "builder.load-built";

// ---- The recipe list, as the pane's own mode --------------------------------------------
//
// WHY A LIST AND NOT ONLY A CYCLE. `builder.recipe` walks the catalog one row at a time and
// says where it landed, which is a fine keyboard gesture and no way at all to CHOOSE with a
// hand: a maker with seven recipes had to press `c` until the one they wanted went past. The
// list is the intelligible visible route -- the catalog, on rows, with the choice on one of
// them -- and it is the pane's own mode for the reason Files' chooser is: a weave has one room
// and no keyboard context of Workshop's.
//
// ⚠ ITS CURSOR IS NOT THE CHOICE. Moving inside the list moves nothing the rest of the pane
// acts on; `builder.recipe-choose` is what makes the row the maker's pick, and
// `builder.recipes-close` leaves the list with the choice exactly as it was. A list whose
// cursor WAS the choice would make merely looking at a recipe arm the next build against it.
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
// ⚠ A MENU THAT DOES NOT CARRY ITS MODE'S CONTROLS BREAKS THE STRIP'S OWN PROMISE. A narrow
// strip drops what will not fit and writes `+N in menu`; the reader's menu offered `close` and
// `manage` and nothing else, so in a thirty-column room the pan, the ends and the two
// neighbouring builds were reachable by no hand at all (the review's fifth finding, B4). Each
// of the reader's operations therefore has a row id of its own here, and the pane's own
// completeness case walks every mode's strip against every mode's menu.
inline constexpr const char* kMenuOutputUp = "builder.menu.output-up";
inline constexpr const char* kMenuOutputDown = "builder.menu.output-down";
inline constexpr const char* kMenuOutputFirst = "builder.menu.output-first";
inline constexpr const char* kMenuOutputLast = "builder.menu.output-last";
inline constexpr const char* kMenuOutputLeft = "builder.menu.output-left";
inline constexpr const char* kMenuOutputRight = "builder.menu.output-right";
inline constexpr const char* kMenuOutputOlder = "builder.menu.output-older";
inline constexpr const char* kMenuOutputNewer = "builder.menu.output-newer";

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
/// half-typed role, which is the honest answer for a mode that was mid-gesture. The output
/// reader is the same: a reloaded pane is not reading, and what the build said is still the
/// tool's to answer when the maker opens it again.
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
