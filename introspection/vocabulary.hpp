// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_VOCABULARY_HPP
#define ZENGINE_INTROSPECTION_VOCABULARY_HPP

// The Introspection tool's DURABLE NAMES, and there are only four of them.
//
// It publishes no vocabulary of its own -- no shape, no schema, no request and no
// answer. Everything it says is somebody else's sentence: the Workshop pane
// protocol's four shapes (workshop/pane_vocabulary.hpp) and the Weave Manager's
// `zen.ListLoaded`. So what a consumer needs from this header is exactly the two
// halves of one durable `PaneRef` plus the two lines a maker reads in the picker.
//
// WHY THE OFFICE IS HERE AND NOT ONLY IN THE .cpp. A `PaneRef` is what a saved
// setup names, so `zengine.introspection/loaded` is a promise to a maker's file --
// it survives this build, this incarnation and this load order, and it is the one
// thing about this tool a later phase must not casually rename. A constant a host,
// a test and the provider all read is what keeps three copies of that promise from
// drifting into two.

namespace zengine::introspection {

/// THE OFFICE THIS TOOL HOLDS -- the durable half of its `PaneRef`, and the only
/// address by which anything reaches it.
///
/// A ROLE, so Workshop's `PaneRoom` finds whoever holds it rather than the
/// incarnation that happened to offer first: a reloaded provider is a different
/// `WeaveId` and the same office (WP-0's B2).
inline constexpr const char* kIntrospectionRole = "zengine.introspection";

/// THE PANE KEY, in this office's namespace and nobody else's. Two offices may
/// both offer `loaded` and they are two panes; that is the `PaneRef` doing its job.
///
/// It names the FACTS rather than the tool, because a second Introspection pane is
/// a plausible later phase and `zengine.introspection/introspection` would have
/// left it nowhere to go.
inline constexpr const char* kLoadedPane = "loaded";

/// The two lines the picker lists, and what Workshop's pane header says after the
/// office (`Loaded @zengine.introspection`).
///
/// Bounded by Workshop's own admission law before a byte is retained -- a name at
/// 32 bytes, a summary at 64 -- so these are written short deliberately rather
/// than trimmed by somebody else later.
///
/// AND SHORTER THAN THE ADMISSION BOUND ON PURPOSE. The picker's name column is TEN
/// cells (`kPickerNameCols`), which is a third of what admission allows, so a name
/// this tool was entitled to would still have arrived at a maker's eye cut. INTR-0
/// found that with `Loaded Weaves` on its first live run and did two things about
/// it: taught the column to MARK its cut, which every later provider now inherits,
/// and then chose a name that does not need the mark. A tool whose name only reads
/// correctly because a truncation is marked has a name too long for the room it
/// lives in.
///
/// `Loaded` rather than `Weaves`, and the difference is this pane's whole caveat:
/// the list is what the kernel LOADED, and a heading reading `Weaves` would promise
/// exactly the population it cannot see.
inline constexpr const char* kLoadedPaneName = "Loaded";
inline constexpr const char* kLoadedPaneSummary = "what the kernel has loaded, and each one's role";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable
/// the way the two keys above are -- it is a file name, and it is here because the
/// host's boot list and the suite's loader must agree on it.
inline constexpr const char* kIntrospectionStem = "zengine-introspection";

} // namespace zengine::introspection

#endif // ZENGINE_INTROSPECTION_VOCABULARY_HPP
