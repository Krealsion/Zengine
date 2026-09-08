// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INFO_PANE_VOCABULARY_HPP
#define ZENGINE_INFO_PANE_VOCABULARY_HPP

// The Info pane's DURABLE NAMES -- the office it holds, the one pane it offers, the action
// ids its keys answer to, and the state a same-shape reload keeps.
//
// WHY THE OFFICE IS NOT `zengine.workshop`. This pane shows the host's own document, so the
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
// reads their objects and properties in, and it is where a fresh session's Workshop puts its
// own material. That is a placement, not a protocol fact -- the placement lives in the
// catalog row Workshop mints from the offer -- but it is why this migration is felt more than
// the other three.
//
// WHY THREE OF THE FIVE IDS ARE THE BUILT-IN'S AND TWO ARE NEW. `info.up`, `info.down` and
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

namespace zengine::info_pane {

/// THE OFFICE THIS PANE HOLDS -- the durable half of its `PaneRef`, and the only address
/// anything reaches it by. A ROLE, so Workshop's `PaneRoom` finds whoever holds it rather
/// than the incarnation that offered first.
inline constexpr const char* kInfoPaneRole = "zengine.info";

/// THE PANE KEY, in this office's namespace. The built-in it replaces carried this same key
/// under `zengine.workshop`; the conversion moves the office and keeps the key.
inline constexpr const char* kInfoPane = "info";

/// THE TWO LINES A MAKER READS IN THE PICKER, and what Workshop's pane header says after the
/// office. They are the built-in's own two lines, unchanged.
inline constexpr const char* kInfoPaneName = "Info";
inline constexpr const char* kInfoPaneSummary = "objects and properties";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable the way the
/// office is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kInfoPaneStem = "zengine-info-pane";

// ---- THE ACTIONS THE PANE DECLARES (`PaneActions`, workshop/pane_vocabulary.hpp) ---------

inline constexpr const char* kActionUp = "info.up";     ///< the property cursor, up
inline constexpr const char* kActionDown = "info.down"; ///< ...and down
inline constexpr const char* kActionEdit = "info.edit"; ///< open a draft on the cursor's row

/// THE DRAFT'S TWO, DECLARED ONLY WHILE ONE IS OPEN (WL-FILES-16's rule): a pane is ONE
/// keyboard context, so while a maker is typing, these two are the only rows this pane
/// declares and every other key reaches it as an ordinary `PaneKey` for the line to consume.
inline constexpr const char* kActionCommit = "info.commit";
inline constexpr const char* kActionCancel = "info.cancel";

/// THE STATE A SAME-SHAPE RELOAD KEEPS (RELOAD-1).
///
/// ONE FIELD, AND IT IS THE ONLY THING HERE THAT IS THE MAKER'S POSITION. Everything this
/// pane SHOWS is the host's reading, re-said the moment it changes, so keeping a copy would
/// make this pane a second owner of the document
/// (`agents/decisions/a-presentation-owns-no-facts.md`).
///
/// ⚠ THE CURSOR IS KEPT HERE AND WAS NOT KEPT BY THE ATTENTION PANE, and the difference is
/// the population rather than a preference. Attention's rows are ranked by loudness and
/// reorder wholesale when anything resolves, so an index carried across a reload would point
/// into a list the new image has not seen. An object's properties are a stable list in a
/// stable order -- name, context, the four extents, the resolved size -- so the row a maker
/// was standing on is still that row, and landing them back on it is worth one integer.
///
/// AND THE DRAFT IS NOT HERE. It is work in flight, and a reload is entitled to drop it: the
/// Builder's role line makes the same trade for the same reason. Nothing was written, because
/// a draft is a draft precisely so that nothing is.
struct InfoPaneState {
    std::int64_t cursor = 0; ///< which property row the maker is on
    ZEN_SHAPE(InfoPaneState, 1, ZEN_FIELD(cursor));
};

} // namespace zengine::info_pane

#endif // ZENGINE_INFO_PANE_VOCABULARY_HPP
