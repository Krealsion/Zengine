// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPOSER_VOCABULARY_HPP
#define ZENGINE_COMPOSER_VOCABULARY_HPP

// The Message Composer's DURABLE NAMES -- five constants, and NO SHAPE OF ITS OWN.
//
// THE ABSENCE IS THE HEADLINE. Introspection needed one shape (`LoadedSelected`)
// because it learned a fact nobody else could say for it. This tool learns
// nothing: it hears somebody else's fact, asks the Loom's own question
// (`zen.DescribeAccepted`), reads the Loom's own answer (`zen.AcceptedShapes`),
// speaks Workshop's own pane sentences, and finally sends a message whose shape
// belongs to whoever it is addressed to. Every sentence in its whole life was
// already in somebody's vocabulary, which is the strongest evidence available
// that MSG-1 left nothing missing for a composer to invent.
//
// So what a consumer needs from this header is the two halves of one durable
// `PaneRef` and the two lines a maker reads in the picker. There is no listener
// interface, because nothing here publishes anything to listen to.
//
// WHY THE OFFICE IS HERE AND NOT ONLY IN THE .cpp: a `PaneRef` is what a saved
// setup names, so `zengine.composer/compose` is a promise to a maker's file. A
// constant a host, a test and the provider all read is what keeps three copies of
// that promise from drifting into two.

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
/// pane (a reply inspector, a history of what this pane submitted) is a plausible
/// later phase and `zengine.composer/composer` would have left it nowhere to go.
inline constexpr const char* kComposePane = "compose";

/// The two lines the picker lists, and what Workshop's pane header says after the
/// office (`Compose @zengine.composer`).
///
/// SEVEN CHARACTERS, AGAINST A TEN-CELL PICKER COLUMN (`kPickerNameCols`) AND A
/// THIRTY-TWO-BYTE ADMISSION BOUND. INTR-0 taught the column to mark its own cut
/// and then chose a name that does not need the mark; this is the second tool to
/// pay that lesson rather than rediscover it.
///
/// `Compose` rather than `Messages`, and the difference is this pane's whole
/// posture: it does not show you the messages a target has received, sent or can
/// answer -- it is where one is WRITTEN.
inline constexpr const char* kComposePaneName = "Compose";
inline constexpr const char* kComposePaneSummary =
    "write a message from a target's own accepted shapes";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable
/// the way the two keys above are -- it is a file name, and it is here because the
/// host's boot list and the suite's loader must agree on it.
inline constexpr const char* kComposerStem = "zengine-composer";

} // namespace zengine::composer

#endif // ZENGINE_COMPOSER_VOCABULARY_HPP
