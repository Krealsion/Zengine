// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_VOCABULARY_HPP
#define ZENGINE_INVENTORY_VOCABULARY_HPP

// THE INVENTORY'S WIRE SHAPES. One slot: empty, or one associated item+metadata pair (see
// codec.hpp for the pair's own encoding). Set replaces the pair as one operation; Get returns
// the pair or an understandable empty result. A rejected Set leaves the previous pair intact.
//
// InventorySet/InventoryGet/InventoryState never name the item's or a metadata entry's schema --
// that is the whole point of the `pair` envelope (codec.hpp): a new item schema is a new set of
// bytes this weave never inspects, never a recompile. InventoryCaptureDescribe is the one
// exception, and deliberately a narrow one: a bootstrap capture source, not a general grabber
// (see weave.hpp for why zen.PokeDescribe/zen.PokeStructure earns this).

#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::inventory {

/// The office this weave holds, so a caller addresses it by role rather than by WeaveId --
/// `zengine.input`, `zengine.skin` and `zengine.guests`' own convention.
inline constexpr const char* kInventoryRole = "zengine.inventory";

/// Replace the slot's pair with `pair` -- the whole encoded envelope `codec.hpp` produces.
/// Answered with `loom::Ack` on success or `loom::Refused` naming why; a refused Set changes
/// nothing.
struct InventorySet {
    loom::Bytes pair;
    ZEN_SHAPE(InventorySet, 1, ZEN_FIELD(pair));
};

/// Ask for the slot's current pair. Fieldless, and never refused merely for being empty --
/// `InventoryState.occupied` is the honest word for that.
struct InventoryGet {
    ZEN_SHAPE(InventoryGet, 1);
};

/// The slot's contents. `occupied` is false and `pair` empty before the first successful Set;
/// this is the understandable empty result the contract asks for, not an absent reply.
struct InventoryState {
    bool occupied = false;
    loom::Bytes pair;
    ZEN_SHAPE(InventoryState, 1, ZEN_FIELD(occupied), ZEN_FIELD(pair));
};

/// THE CAPTURE ADAPTER'S DOOR: ask `target_role`'s current holder to describe its own structure
/// (`zen.PokeDescribe` -- the self-description floor every woven Weave already answers,
/// unconditionally), and Set the resulting `zen.PokeStructure` as the item, with one
/// automatically derived `CaptureContext` metadata entry recording the actual request and the
/// answering weave Loom attested. Answered like Set: `loom::Ack` on success, `loom::Refused`
/// naming why (including a target that never answers PokeDescribe, or answers something else).
struct InventoryCaptureDescribe {
    std::string target_role;
    ZEN_SHAPE(InventoryCaptureDescribe, 1, ZEN_FIELD(target_role));
};

/// Automatically produced by InventoryCaptureDescribe -- one real structured metadata entry, not
/// a manually assembled demonstration. `requested_role` is a supplier claim (what the caller
/// asked this weave to query); `request_shape`/`request_version` name what this weave actually
/// sent; `answered_by` is the WeaveId Loom itself attested as the sender of the PokeStructure
/// reply -- an OBSERVED fact, never read from the reply's own payload, and never a durable
/// identity across a restart or a role that changes hands. `captured_at_epoch_s` is this
/// process's own clock at the moment the answer settled: a local reading, not a claim that any
/// other observation in this pair was simultaneous with it.
struct CaptureContext {
    std::string requested_role;
    std::string request_shape;
    std::int64_t request_version = 0;
    std::string answered_by;
    std::int64_t captured_at_epoch_s = 0;
    ZEN_SHAPE(CaptureContext, 1, ZEN_FIELD(requested_role), ZEN_FIELD(request_shape),
              ZEN_FIELD(request_version), ZEN_FIELD(answered_by), ZEN_FIELD(captured_at_epoch_s));
};

} // namespace zengine::inventory

#endif // ZENGINE_INVENTORY_VOCABULARY_HPP
