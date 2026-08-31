// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_MIGRATION_FAMILY_HPP
#define ZENGINE_TESTS_MIGRATION_FAMILY_HPP

// A DURABLE SHAPE WITH A HISTORY, INVENTED FOR THE SUITE (MIG-0) — one name, three
// versions, and edges between them that say WHICH ROAD RAN.
//
// It is a fixture and not a product shape on purpose. What the migration seam has to be
// asked about is arrangements the real session history deliberately does not contain: two
// edges that meet in the middle with no direct one; a contribution whose NAME says one edge
// while its schemas say another; a conversion answering the right name at the wrong shape.
// Asking those of `WorkshopSession` would mean shipping dishonest session converters.
//
// `rungs` IS THE INSTRUMENT. Every edge writes how many rungs the value climbed, so a test
// can tell a DIRECT `v1 -> v3` (1) from a chain through v2 (2) from a composed one (7) from
// a replacement (99) — by reading the answer rather than by trusting that the identity it
// asked for is the identity that ran.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace mig_fixture {

/// How many rungs each authored road records, so an answer names its own path.
inline constexpr std::int64_t kDirectRungs = 1;
inline constexpr std::int64_t kViaV2Rungs = 2;
inline constexpr std::int64_t kComposedRungs = 7;
inline constexpr std::int64_t kReplacedRungs = 99;

/// The word a version-1 value carries forward when a road invents the field v1 never had.
inline const std::string& v1_note() {
    static const std::string note = "from v1";
    return note;
}

namespace v1 {

/// The oldest vintage: a number and nothing else.
struct Rung {
    std::int64_t carried = 0;

    ZEN_SHAPE(Rung, 1, ZEN_FIELD(carried));
};

} // namespace v1

namespace v2 {

/// The middle vintage: the number, and a note the first vintage had no word for.
struct Rung {
    std::int64_t carried = 0;
    std::string note;

    ZEN_SHAPE(Rung, 2, ZEN_FIELD(carried), ZEN_FIELD(note));
};

} // namespace v2

namespace v3 {

/// The current vintage — what a reader of this family admits today.
struct Rung {
    std::int64_t carried = 0;
    std::string note;
    std::int64_t rungs = 0;

    ZEN_SHAPE(Rung, 3, ZEN_FIELD(carried), ZEN_FIELD(note), ZEN_FIELD(rungs));
};

} // namespace v3

} // namespace mig_fixture

#endif // ZENGINE_TESTS_MIGRATION_FAMILY_HPP
