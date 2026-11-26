// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NOTICES_PANE_VOCABULARY_HPP
#define ZENGINE_NOTICES_PANE_VOCABULARY_HPP

// The Notices pane's durable names: the role it holds, the one pane it offers, the action ids its
// keys answer to, and the state a same-shape reload keeps. The record it reads is the notices
// role's (notices/vocabulary.hpp), and its laws are notices/docs/notices-laws.md.

#include <zen/weave/shape.hpp>

#include <cstdint>

namespace zengine::notices_pane {

/// The role this pane holds, and the only address by which anything reaches it.
inline constexpr const char* kNoticesPaneRole = "zengine.notices-pane";

/// The pane key, in this role's namespace.
inline constexpr const char* kNoticesPane = "notices";

/// Its name in the Pane Manager's list and its summary in Info, inside Workshop's admission bounds:
/// a name of at most 32 bytes, a summary of at most 64.
inline constexpr const char* kNoticesPaneName = "Notices";
inline constexpr const char* kNoticesPaneSummary = "what Workshop said";

/// The library stem a host boots. It is here because the host's plan and the suites' loader must
/// agree on it.
inline constexpr const char* kNoticesPaneStem = "zengine-notices-pane";

/// The actions the pane declares: one notice older, one notice newer. The wheel's notches ask the
/// same two, so a key and a notch move the same rows.
inline constexpr const char* kActionUp = "notices.up";
inline constexpr const char* kActionDown = "notices.down";

/// What a same-shape reload keeps: the notice the view ends at, by its number, 0 while it follows
/// the newest.
struct NoticesPaneState {
    std::int64_t newest = 0;
    ZEN_SHAPE(NoticesPaneState, 1, ZEN_FIELD(newest));
};

} // namespace zengine::notices_pane

#endif // ZENGINE_NOTICES_PANE_VOCABULARY_HPP
