// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NOTICES_PANE_VOCABULARY_HPP
#define ZENGINE_NOTICES_PANE_VOCABULARY_HPP

// The Notices pane's durable names: the office it holds, the one pane it offers, the action ids its
// keys answer to, and the state a same-shape reload keeps. The record it walks is the notices
// office's (notices/vocabulary.hpp).
// Workshop law: agents/workshop/notices.md

#include <zen/weave/shape.hpp>

#include <cstdint>

namespace zengine::notices_pane {

/// THE OFFICE THIS PANE HOLDS, and the only address anything reaches it by.
inline constexpr const char* kNoticesPaneRole = "zengine.notices-pane";

/// THE PANE KEY, in this office's namespace.
inline constexpr const char* kNoticesPane = "notices";

/// ITS NAME IN THE PANE MANAGER'S LIST AND ITS SUMMARY IN INFO, inside Workshop's admission bounds:
/// a name at 32 bytes, a summary at 64.
inline constexpr const char* kNoticesPaneName = "Notices";
inline constexpr const char* kNoticesPaneSummary = "what Workshop said";

/// THE LIBRARY STEM A HOST BOOTS, here because the host's plan and the suites' loader must agree.
inline constexpr const char* kNoticesPaneStem = "zengine-notices-pane";

/// THE ACTIONS THE PANE DECLARES: one notice older, one notice newer. The wheel's notches ask the
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
