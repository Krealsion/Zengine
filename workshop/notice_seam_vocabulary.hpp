// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_NOTICE_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_NOTICE_SEAM_VOCABULARY_HPP

// What Workshop said, said across the bus as it says it: each notice published once, at the end of
// the delivery that said it, for the notices record to keep (notices/vocabulary.hpp) and the
// Notices pane to show. A thing said is a moment that passed; what is true now is
// `StandingConditions`'s (WL-ATTN-01).
// Workshop law: agents/workshop/notices.md

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace zengine::workshop {

/// THE MOST NOTICES ONE DELIVERY HOLDS TO PUBLISH AT ITS END: past it, the oldest owed is let go.
inline constexpr std::size_t kMaxNoticesOwed = 256;

/// ONE THING WORKSHOP SAID, published to any by `zengine.workshop`, once: its number in this run,
/// from 1, the words, and whether it was a refusal.
// WL-NOTE-01 -- agents/workshop/notices.md
struct NoticeSaid {
    std::int64_t number = 0;
    std::string text;
    bool refusal = false;
    ZEN_SHAPE(NoticeSaid, 1, ZEN_FIELD(number), ZEN_FIELD(text), ZEN_FIELD(refusal));
};

/// A SEATED PANE ASKS WORKSHOP TO SAY A REFUSAL WHOLE. `text` is the sentence the pane's row
/// paints cut at the pane's width. Workshop says it as its own notice, a refusal, and answers
/// nothing. An empty sentence, or one from a role that holds no seated pane, is not said.
struct PaneRefusalRequested {
    std::string text;
    ZEN_SHAPE(PaneRefusalRequested, 1, ZEN_FIELD(text));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_NOTICE_SEAM_VOCABULARY_HPP
