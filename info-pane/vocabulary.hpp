// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INFO_PANE_VOCABULARY_HPP
#define ZENGINE_INFO_PANE_VOCABULARY_HPP

// The Info pane's DURABLE NAMES -- the office it holds, the one pane it offers, the action
// ids its keys answer to, and the state a same-shape reload keeps.
//
// WHAT IT INSPECTS. Panes: the one inventory the host says, and one pane of it as a SUBJECT the
// maker names here -- its identity, where the desk places it, what the screen made of that, and
// (for a pane a maker made) its region -- with a property edit written by the pane's owner
// (`workshop/inspection_seam_vocabulary.hpp`). It inspected the prototype object document
// before that document retired; the office, the key and the draft discipline are the same.
//
// WHY THE OFFICE IS NOT `zengine.workshop`. This pane shows the host's own rows, so the
// host's office would have looked like the honest address -- and it is exactly the address a
// migrated pane may not have: `zengine.workshop` is the host's singleton role and admission
// refuses a pane offered by whoever holds it as a forgery (WL-CAT-03).
//
// WHY THE PANE KEY IS STILL `info`. The built-in was `zengine.workshop/info`, and a saved
// setup naming it is converted to `zengine.info/info` at load
// (`workshop/pane_migration.hpp`): the OFFICE moves, the pane key does not. Keeping the key
// is what lets one conversion move a maker's desk without touching where on it the pane sits.
//
// ⚠ AND THIS PANE IS THE SIDE REGION, which no migrated pane has been before. Files and the
// Builder were overlay-stack panes and Attention was chrome; `Info` is the column a maker
// reads a pane's facts and placement in, and it is where a fresh session's Workshop puts its
// own material. That is a placement, not a protocol fact -- the placement lives in the
// catalog row Workshop mints from the offer -- but it is why this migration is felt more than
// the other three.
//
// WHY THREE OF THE SIX IDS ARE THE BUILT-IN'S AND THREE ARE NEW (`info.switch` arrived with the
// pane list, below). `info.up`, `info.down` and
// `info.edit` were WORKSHOP command-mode rows and are the pane's now, spelled and defaulted
// exactly as they were, so a maker's authored override keeps working. The draft's two --
// commit and cancel -- could NOT keep `draft.commit` / `draft.cancel`: those rows are
// `KeyContext::kDraft`'s and the Pane Manager still declares them for ITS drafts, so the host
// keeps them and `join_pane_rows` would refuse a pane that claimed the same ids (the id law
// and the collision law). They are `info.commit` and `info.cancel` here, on the same gestures,
// and a maker who moved `draft.commit` finds it moved for the Pane Manager and not here --
// which is a real loss, named here because it is the price of the two panes sharing one
// keyboard context before either was a weave.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::info_pane {

/// THE OFFICE THIS PANE HOLDS -- the durable half of its `PaneRef`, and the only address
/// anything reaches it by. A ROLE, so Workshop's `PaneRoom` finds whoever holds it rather
/// than the incarnation that offered first.
inline constexpr const char* kInfoPaneRole = "zengine.info";

/// THE PANE KEY, in this office's namespace. The built-in it replaces carried this same key
/// under `zengine.workshop`; the conversion moves the office and keeps the key.
inline constexpr const char* kInfoPane = "info";

/// THE TWO LINES A MAKER READS ABOUT THIS PANE -- its name in the Pane Manager's list, its
/// summary in Info -- and what Workshop's pane header says after the office. They are the
/// built-in's own two lines, unchanged.
inline constexpr const char* kInfoPaneName = "Info";
inline constexpr const char* kInfoPaneSummary = "the panes, and one pane's properties";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable the way the
/// office is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kInfoPaneStem = "zengine-info-pane";

// ---- THE ACTIONS THE PANE DECLARES (`PaneActions`, workshop/pane_vocabulary.hpp) ---------

inline constexpr const char* kActionUp = "info.up";     ///< the cursor of the list with the keys, up
inline constexpr const char* kActionDown = "info.down"; ///< ...and down
/// RETURN: on the pane list, inspect the pane under the cursor; on the properties, open a
/// draft on the cursor's row. One id, labelled for the list it answers in.
inline constexpr const char* kActionEdit = "info.edit";
/// TAB: move the keys between the pane list and the subject's properties.
inline constexpr const char* kActionSwitch = "info.switch";

/// THE DRAFT'S TWO, DECLARED ONLY WHILE ONE IS OPEN (WL-FILES-16's rule): a pane is ONE
/// keyboard context, so while a maker is typing, these two are the only rows this pane
/// declares and every other key reaches it as an ordinary `PaneKey` for the line to consume.
inline constexpr const char* kActionCommit = "info.commit";
inline constexpr const char* kActionCancel = "info.cancel";

/// THE STATE A SAME-SHAPE RELOAD KEEPS (RELOAD-1): the maker's POSITION, and nothing shown.
///
/// Everything this pane SHOWS is the host's reading, re-said the moment it changes, so keeping a
/// copy would make this pane a second owner of the desk
/// (`agents/decisions/a-presentation-owns-no-facts.md`). The subject is not here either: the
/// host records which pane this office inspects, so a reload finds it standing.
///
/// THE LIST CURSOR IS AN IDENTITY, NOT AN INDEX (WL-DESK-10's rule, one pane over): a pane
/// inserted above it moves the marker with it, and a pane that left the list leaves the marker
/// holding nothing rather than on whichever pane slid into its place. The property cursor is an
/// index, because a pane's rows are a stable list in a stable order.
///
/// (!) AND THE ABSENCE OF A CURRENT CHOICE IS STATE TOO. A chosen pane that left the list keeps
/// its keys here, so the image a reload hands this to still knows the choice is lost, and Return
/// inspects nothing until a row is chosen; both keys empty means only that nothing was ever
/// chosen. The fields are version 2's, unchanged -- a same-shape reload carries them -- but an
/// image from before this rule cleared the keys of a lost choice, and what one of those hands
/// over reads as never chosen, once.
///
/// AND THE DRAFT IS NOT HERE. It is work in flight, and a reload is entitled to drop it: the
/// Builder's role line makes the same trade for the same reason. Dropping it loses only what was
/// never written: a commit it already sent is the owner's either way.
///
/// (!) VERSION 2, AND A LIVE RELOAD FROM VERSION 1 IS REFUSED (RELOAD-1): the Info that showed
/// the object document kept one integer. A Workshop still running that image takes this one at
/// its next launch.
struct InfoPaneState {
    std::int64_t cursor = 0; ///< which property row the maker is on
    std::string list_office; ///< which pane the list cursor holds, by identity -- kept when it
    std::string list_pane;   ///< leaves the list; both empty until a row is first held
    bool on_panes = true;    ///< the keys are in the pane list (true) or the properties
    ZEN_SHAPE(InfoPaneState, 2, ZEN_FIELD(cursor), ZEN_FIELD(list_office), ZEN_FIELD(list_pane),
              ZEN_FIELD(on_panes));
};

} // namespace zengine::info_pane

#endif // ZENGINE_INFO_PANE_VOCABULARY_HPP
