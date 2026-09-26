// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPOSER_VOCABULARY_HPP
#define ZENGINE_COMPOSER_VOCABULARY_HPP

// The Message Composer's durable names: five constants, and no shape of its own. This tool
// learns nothing -- it hears somebody else's fact, asks the Loom's question
// (`zen.DescribeAccepted`), reads the Loom's answer, speaks Workshop's pane sentences and sends
// a message in its target's shape -- so a consumer needs only the two halves of one durable
// `PaneRef` (a promise to a maker's saved setup) and the two lines a maker reads about the pane.
// Pane law: agents/panes.md

namespace zengine::composer {

/// THE OFFICE THIS TOOL HOLDS -- the durable half of its `PaneRef`, and the only
/// address by which anything reaches it.
///
/// A ROLE, so Workshop's `PaneRoom`, `PanePressed`, `PaneKey` and `PaneTextInput`
/// find whoever holds it rather than the incarnation that happened to offer first.
inline constexpr const char* kComposerRole = "zengine.composer";

/// THE PANE KEY, in this office's namespace and nobody else's.
///
/// It names the ACT rather than the tool, for `loaded`'s reason: a second Composer
/// pane (a reply inspector, a history of what this pane submitted) is plausible,
/// and `zengine.composer/composer` would leave it nowhere to go.
inline constexpr const char* kComposePane = "compose";

/// The two lines a maker reads about the pane: its name in the Pane Manager's list and its
/// summary in Info, also what Workshop's pane header says after the office (`Compose
/// @zengine.composer`). Short, for a thirty-two-byte admission bound and a narrow column;
/// `Compose` rather than `Messages`: this is where a message is written, not where one is shown.
inline constexpr const char* kComposePaneName = "Compose";
inline constexpr const char* kComposePaneSummary =
    "write a message from a target's own accepted shapes";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable
/// the way the two keys above are -- it is a file name, and it is here because the
/// host's boot list and the suite's loader must agree on it.
inline constexpr const char* kComposerStem = "zengine-composer";

} // namespace zengine::composer

#endif // ZENGINE_COMPOSER_VOCABULARY_HPP
