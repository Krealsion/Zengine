// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NOTICES_VOCABULARY_HPP
#define ZENGINE_NOTICES_VOCABULARY_HPP

// The notices record's durable names: the role that keeps what Workshop said, its one setting and
// its bounds, the ask that reads a page of it, the page, the message that says it moved, and the
// state a reload keeps. What Workshop says arrives as `NoticeSaid`
// (workshop/notice_seam_vocabulary.hpp). The laws these keep are notices/docs/notices-laws.md.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::notices {

/// The role that keeps the notices, and the only address by which anything reaches the record.
inline constexpr const char* kNoticesRole = "zengine.notices";

/// The library stem a host boots. It is here because the host's plan and the suites' loader must
/// agree on it.
inline constexpr const char* kNoticesStem = "zengine-notices";

/// The one setting the record declares, role-wide: how many notices it keeps, the oldest let go
/// past it. It takes `kDefaultKept` to `kMostKept`.
inline constexpr const char* kSettingKeep = "keep";
inline constexpr std::int64_t kDefaultKept = 100;
inline constexpr std::int64_t kMostKept = 1000;

/// The longest notice kept, in bytes: a longer one is kept cut there, ending `...`.
inline constexpr std::size_t kMaxNoticeBytes = 1024;

/// One page carries at most this many notices, whatever the ask counts.
inline constexpr std::int64_t kMaxNoticesPage = 200;

/// One notice kept: its number in the run, the words as kept, and whether it was a refusal.
struct KeptNotice {
    std::int64_t number = 0;
    std::string text;
    bool refusal = false;
    ZEN_SHAPE(KeptNotice, 1, ZEN_FIELD(number), ZEN_FIELD(text), ZEN_FIELD(refusal));
};

/// Asks for a page of the record, answered to the asker alone: up to `count` notices, the newest of
/// them the newest kept at or before number `newest`, or the newest kept when `newest` is 0.
struct NoticesRequested {
    std::int64_t newest = 0;
    std::int64_t count = 0;
    ZEN_SHAPE(NoticesRequested, 1, ZEN_FIELD(newest), ZEN_FIELD(count));
};

/// The page: the notices asked for, oldest first; how many kept notices stand before and after
/// them, and the number of the first after them (0 when none); the oldest and newest kept numbers
/// (0 when nothing is kept); how many the record keeps; and how many it has let go this run.
struct NoticesShown {
    std::vector<KeptNotice> rows;
    std::int64_t before = 0;
    std::int64_t after = 0;
    std::int64_t next = 0;
    std::int64_t first = 0;
    std::int64_t last = 0;
    std::int64_t keep = 0;
    std::int64_t let_go = 0;
    ZEN_SHAPE(NoticesShown, 1, ZEN_FIELD(rows), ZEN_FIELD(before), ZEN_FIELD(after),
              ZEN_FIELD(next), ZEN_FIELD(first), ZEN_FIELD(last), ZEN_FIELD(keep),
              ZEN_FIELD(let_go));
};

/// Says the record moved, published to any when it kept a notice or let some go, and on every
/// activation: the oldest and newest kept numbers, how many it keeps, and how many it has let go.
/// It carries no words; a reader asks for the page it wants.
struct NoticesMoved {
    std::int64_t first = 0;
    std::int64_t last = 0;
    std::int64_t keep = 0;
    std::int64_t let_go = 0;
    ZEN_SHAPE(NoticesMoved, 1, ZEN_FIELD(first), ZEN_FIELD(last), ZEN_FIELD(keep),
              ZEN_FIELD(let_go));
};

/// What a same-shape reload keeps: the notices kept, oldest first, and how many were let go. It is
/// never written to a file: the record lasts the run.
struct NoticesState {
    std::vector<KeptNotice> kept;
    std::int64_t let_go = 0;
    ZEN_SHAPE(NoticesState, 1, ZEN_FIELD(kept), ZEN_FIELD(let_go));
};

} // namespace zengine::notices

#endif // ZENGINE_NOTICES_VOCABULARY_HPP
