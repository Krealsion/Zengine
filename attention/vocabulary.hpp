// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_ATTENTION_PANE_VOCABULARY_HPP
#define ZENGINE_ATTENTION_PANE_VOCABULARY_HPP

// The Attention pane's durable names: the role it holds, the one pane it offers, the action ids its
// keys answer to, and the state a same-shape reload keeps. The role is `zengine.attention`, not the
// host's `zengine.workshop` (admission refuses a pane offered by its holder, WL-CAT-03). No saved
// file names this pane, which is no catalog row, so nothing converts. A weaver's override of
// `attention.close` or `workshop.attention`, ids nothing declares, is kept at load and bound to
// nothing (WL-KEY-06). The laws of what earns attention are attention/docs/attention-laws.md.

// The action ids keep the spellings and keys weavers' overrides name. They are the pane's alone:
// Workshop declares no row of its own for them (WL-KEY-06, WL-KEY-08).

#include <zen/weave/shape.hpp>

#include <string>
#include <vector>

namespace zengine::attention_pane {

/// The role this pane holds -- the durable half of its `PaneRef`, and the only address
/// anything reaches it by. A role, so the window and Workshop find whoever holds it rather
/// than the incarnation that offered first: a reloaded provider is a different `WeaveId` and
/// the same role.
inline constexpr const char* kAttentionPaneRole = "zengine.attention";

/// The pane key, in this role's namespace.
inline constexpr const char* kAttentionPane = "attention";

/// The two lines a weaver reads about this pane -- its name in the Pane Manager's list, its
/// summary in Info -- and what Workshop's pane header says after the role.
/// Bounded by Workshop's admission law before a byte is retained: a name at 32 bytes, a
/// summary at 64.
inline constexpr const char* kAttentionPaneName = "Attention";
inline constexpr const char* kAttentionPaneSummary = "what is true right now";

/// The library stem a host boots. Not part of the pane protocol and not durable the way the
/// role is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kAttentionPaneStem = "zengine-attention-pane";

// ---- The actions the pane declares (`PaneActions`, pane/vocabulary.hpp) ---------
//
// The ids a weaver's keymap file names to move them, in the spellings weavers' overrides name.
// What each one does is the weave's (pane.cpp); what key requests it is
// Workshop's effective keymap, and the weave is told the id, never the key.

inline constexpr const char* kActionUp = "attention.up";
inline constexpr const char* kActionDown = "attention.down";
inline constexpr const char* kActionDismiss = "attention.dismiss";

/// The one setting the pane declares (`v2::PaneSettingsDeclared`, pane/settings.hpp):
/// whether the cursor's condition is explained beneath it in a layout, on unless the layout keeps
/// it off.
inline constexpr const char* kSettingDetail = "detail";

/// One condition the weaver has hidden, and the statement they hid. `stamp` is the content, not a
/// time: remembering only the key would hide a later, different condition wearing the same name,
/// so a condition that materially changes is visible again with nobody clearing anything
/// (WL-ATTN-08).
struct Dismissal {
    std::string key;
    std::string stamp;
    ZEN_SHAPE(Dismissal, 1, ZEN_FIELD(key), ZEN_FIELD(stamp));
};

/// The state a same-shape reload keeps: which statements the weaver chose not to look at, the one
/// thing here that is theirs (`agents/decisions/a-presentation-owns-no-facts.md`). Not the
/// cursor: it is a position in a derived population that can shrink, so a reload lands on the
/// first, loudest row. Nor is it written to a file: a shape crosses a reload in memory, and the
/// dismissal set lasts the session.
struct AttentionPaneState {
    std::vector<Dismissal> dismissed;
    ZEN_SHAPE(AttentionPaneState, 1, ZEN_FIELD(dismissed));
};

} // namespace zengine::attention_pane

#endif // ZENGINE_ATTENTION_PANE_VOCABULARY_HPP
