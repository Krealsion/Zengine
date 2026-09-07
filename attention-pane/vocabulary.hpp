// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_ATTENTION_PANE_VOCABULARY_HPP
#define ZENGINE_ATTENTION_PANE_VOCABULARY_HPP

// The Attention pane's DURABLE NAMES -- the office it holds, the one pane it offers, the
// action ids its keys answer to, and the state a same-shape reload keeps.
//
// WHY THE OFFICE IS `zengine.attention` AND NOT `zengine.workshop`. What this pane presents
// is Workshop's own reading of Workshop's own state, so the host's office would have looked
// like the honest address -- and it is exactly the address a migrated pane may not have:
// `zengine.workshop` is the host's singleton role and admission refuses a pane offered by
// whoever holds it as a forgery (WL-CAT-03). The office is the durable half of a `PaneRef`,
// so it is a promise to a maker's saved setup and belongs to the party that OFFERS the pane.
//
// ⚠ AND NO SAVED FILE NAMES THIS PANE, WHICH IS WHY `pane_migration.hpp` GAINS NOTHING.
// Files and the Builder were CATALOG ROWS (`kPanelCatalog`): every setup a maker ever saved
// could hold `zengine.workshop/project-files`, so each migration owed a conversion. The
// current-condition view was never a row of anything -- it was an OVERLAY, opened by a
// global chord, holding a cursor and a dismissal set that were "session-only, never
// persisted" (WL-ATTN-08). There is no yesterday's byte that names it, so there is nothing
// to convert, and inventing a third named pair would have been a conversion for a file that
// cannot exist.
//
// WHY THE ACTION IDS ARE `attention.up`, `attention.down`, `attention.dismiss`. They are the
// built-in's own ids (`workshop/keymap.hpp` `kActionCatalog`, before this migration) under
// the retired `KeyContext::kAttention`, spelled here unchanged so a maker's authored
// `attention.dismiss` keeps the key they moved it to. They were WORKSHOP ids and are the
// PANE's now: legal exactly because the host's rows leave in the same commit, and
// `join_pane_rows` would refuse the declaration whole for as long as both existed
// (WL-KEY-06, WL-KEY-08).
//
// ⚠ TWO IDS RETIRE RATHER THAN MOVE, AND THE LOSS IS NAMED. `attention.close` closed the
// OVERLAY; a pane is not a mode and has nothing to close -- it is removed from the desk
// through the picker, like every other pane. `workshop.attention` was the global chord that
// opened the overlay from anywhere; the pane is opened from the picker, and VD-22 refuses a
// host-mapped route to a weave's presence as firmly as it refuses one to its actions. A
// maker who authored either keeps a row preserved-as-unknown at load (WL-KEY-06), bound to
// nothing -- which is the honest state for a gesture whose subject stopped existing, and is
// better than minting a pane id that would answer a key by doing nothing.

#include <zen/weave/shape.hpp>

#include <string>
#include <vector>

namespace zengine::attention_pane {

/// THE OFFICE THIS PANE HOLDS -- the durable half of its `PaneRef`, and the only address
/// anything reaches it by. A ROLE, so Workshop's `PaneRoom` finds whoever holds it rather
/// than the incarnation that offered first: a reloaded provider is a different `WeaveId` and
/// the same office.
inline constexpr const char* kAttentionPaneRole = "zengine.attention";

/// THE PANE KEY, in this office's namespace.
inline constexpr const char* kAttentionPane = "attention";

/// THE TWO LINES A MAKER READS IN THE PICKER, and what Workshop's pane header says after the
/// office. Bounded by Workshop's admission law before a byte is retained -- a name at 32
/// bytes, a summary at 64 -- and written short deliberately, inside the ten cells the
/// picker's name column actually shows.
inline constexpr const char* kAttentionPaneName = "Attention";
inline constexpr const char* kAttentionPaneSummary = "what is true right now";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable the way the
/// office is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kAttentionPaneStem = "zengine-attention-pane";

// ---- THE ACTIONS THE PANE DECLARES (`PaneActions`, workshop/pane_vocabulary.hpp) ---------
//
// The ids a maker's keymap file names to move them, carrying the built-in's own spellings
// unchanged. What each one DOES is the weave's (pane.cpp); what KEY requests it is
// Workshop's effective keymap, and the weave is told the id, never the key.

inline constexpr const char* kActionUp = "attention.up";
inline constexpr const char* kActionDown = "attention.down";
inline constexpr const char* kActionDismiss = "attention.dismiss";

/// ONE CONDITION THE MAKER HAS HIDDEN, and the statement they hid.
///
/// `stamp` IS THE CONTENT, NOT A TIME. A dismissal that remembered only the key would hide a
/// condition whose words later changed into a different condition wearing the same name; the
/// built-in measured the statement instead and this pane measures the same thing, so a
/// condition that materially changes is visible again with nobody clearing anything
/// (WL-ATTN-08).
struct Dismissal {
    std::string key;
    std::string stamp;
    ZEN_SHAPE(Dismissal, 1, ZEN_FIELD(key), ZEN_FIELD(stamp));
};

/// THE STATE A SAME-SHAPE RELOAD KEEPS (RELOAD-1).
///
/// ONE FIELD, AND IT IS THE ONLY THING HERE THAT IS THE MAKER'S. What this pane SHOWS is the
/// host's own reading, re-said the moment it changes, so keeping a copy would make this pane
/// a second owner of somebody else's facts (`agents/decisions/a-presentation-owns-no-facts.md`).
/// What is genuinely this pane's is which statements the maker has chosen not to look at.
///
/// ⚠ THE CURSOR IS DELIBERATELY NOT HERE. It is a position in a DERIVED population that can
/// shrink between one beat and the next, so a reloaded image carrying a number from before
/// its own reading would be pointing at a row by index into a list it has not seen yet. The
/// same reason the built-in resolved its cursor fresh at every paint rather than clamping it
/// once. A reload lands on the first row, which is the loudest one.
///
/// AND IT IS NOT PERSISTED BY BEING HERE. A weave's shape crosses a RELOAD, in memory, on the
/// same `WeaveId`; nothing writes it to disk. "Session-only, never persisted" is as true of
/// the dismissal set now as it was when the host held it.
struct AttentionPaneState {
    std::vector<Dismissal> dismissed;
    ZEN_SHAPE(AttentionPaneState, 1, ZEN_FIELD(dismissed));
};

} // namespace zengine::attention_pane

#endif // ZENGINE_ATTENTION_PANE_VOCABULARY_HPP
