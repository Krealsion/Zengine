// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_ATTENTION_PANE_VOCABULARY_HPP
#define ZENGINE_ATTENTION_PANE_VOCABULARY_HPP

// The Attention pane's durable names: the office it holds, the one pane it offers, the action
// ids its keys answer to, and the state a same-shape reload keeps. The office is
// `zengine.attention`, not the host's `zengine.workshop` (admission refuses a pane offered by
// its holder, WL-CAT-03). No saved file names this pane -- it was never a catalog row -- so
// nothing converts. `attention.close` and `workshop.attention` retired with the overlay: a
// maker's override of either is kept at load, bound to nothing (WL-KEY-06).
// Workshop law: agents/workshop/attention.md

// The action ids keep the spellings and keys makers' overrides name, and are the pane's now,
// legal because Workshop's own rows left (WL-KEY-06, WL-KEY-08).

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

/// THE TWO LINES A MAKER READS ABOUT THIS PANE -- its name in the Pane Manager's list, its
/// summary in Info -- and what Workshop's pane header says after the office.
/// Bounded by Workshop's admission law before a byte is retained -- a name at 32 bytes, a
/// summary at 64 -- and written short deliberately, to the ten cells the retired picker's
/// name column showed.
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

/// One condition the maker has hidden, and the statement they hid. `stamp` is the content, not a
/// time: remembering only the key would hide a later, different condition wearing the same name,
/// so a condition that materially changes is visible again with nobody clearing anything
/// (WL-ATTN-08).
struct Dismissal {
    std::string key;
    std::string stamp;
    ZEN_SHAPE(Dismissal, 1, ZEN_FIELD(key), ZEN_FIELD(stamp));
};

/// The state a same-shape reload keeps: which statements the maker chose not to look at, the one
/// thing here that is theirs (`agents/decisions/a-presentation-owns-no-facts.md`). Not the
/// cursor: it is a position in a derived population that can shrink, so a reload lands on the
/// first, loudest row. Not persisted either: a shape crosses a reload in memory, and the
/// dismissal set is session-only, as it always was.
struct AttentionPaneState {
    std::vector<Dismissal> dismissed;
    ZEN_SHAPE(AttentionPaneState, 1, ZEN_FIELD(dismissed));
};

} // namespace zengine::attention_pane

#endif // ZENGINE_ATTENTION_PANE_VOCABULARY_HPP
