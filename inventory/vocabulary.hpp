// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_VOCABULARY_HPP
#define ZENGINE_INVENTORY_VOCABULARY_HPP

// Wire shapes for independent typed entries and the legacy replacement slot. Pair bytes own
// their schema closure and values (codec.hpp). Entry identity is independent of list position;
// references confer no authority. docs/reference/inventory.md owns the public contract.

#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

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

/// Locate the legacy capture slot. An empty slot answers Refused. The returned reference is
/// an identity, not a grant; callers still need authority for each read or write.
struct InventoryLocate {
    ZEN_SHAPE(InventoryLocate, 1);
};

struct InventoryReference {
    std::string owner;
    std::string entry;
    ZEN_SHAPE(InventoryReference, 1, ZEN_FIELD(owner), ZEN_FIELD(entry));
};

struct InventoryRead {
    InventoryReference reference;
    ZEN_SHAPE(InventoryRead, 1, ZEN_FIELD(reference));
};

/// Update this entry only if its revision still matches. Set/Capture replace the entry;
/// Write preserves its identity. A conflict or malformed pair leaves storage untouched.
struct InventoryWrite {
    InventoryReference reference;
    std::int64_t revision = 0;
    loom::Bytes pair;
    ZEN_SHAPE(InventoryWrite, 1, ZEN_FIELD(reference), ZEN_FIELD(revision), ZEN_FIELD(pair));
};

struct InventoryEntry {
    InventoryReference reference;
    std::int64_t revision = 0;
    loom::Bytes pair;
    ZEN_SHAPE(InventoryEntry, 1, ZEN_FIELD(reference), ZEN_FIELD(revision), ZEN_FIELD(pair));
};

/// Collection doors leave the legacy Set/Get capture slot alone. References, rather than
/// display positions, identify entries across sorting, renaming and independent writes.
struct InventoryAdd {
    loom::Bytes pair;
    std::string label;
    ZEN_SHAPE(InventoryAdd, 1, ZEN_FIELD(pair), ZEN_FIELD(label));
};
struct InventoryList { ZEN_SHAPE(InventoryList, 1); };
struct InventorySummary {
    InventoryReference reference;
    std::int64_t revision = 0;
    std::string label;
    std::string schema;
    std::int64_t version = 0;
    bool capture_slot = false;
    ZEN_SHAPE(InventorySummary, 1, ZEN_FIELD(reference), ZEN_FIELD(revision), ZEN_FIELD(label),
              ZEN_FIELD(schema), ZEN_FIELD(version), ZEN_FIELD(capture_slot));
};
struct InventoryListed {
    std::vector<InventorySummary> entries;
    ZEN_SHAPE(InventoryListed, 1, ZEN_FIELD(entries));
};
struct InventoryRename {
    InventoryReference reference;
    std::int64_t revision = 0;
    std::string label;
    ZEN_SHAPE(InventoryRename, 1, ZEN_FIELD(reference), ZEN_FIELD(revision), ZEN_FIELD(label));
};
struct InventoryRemove {
    InventoryReference reference;
    std::int64_t revision = 0;
    ZEN_SHAPE(InventoryRemove, 1, ZEN_FIELD(reference), ZEN_FIELD(revision));
};
/// An invalidation, not a second inventory. Receivers ask List for current summaries.
struct InventoryChanged { ZEN_SHAPE(InventoryChanged, 1); };

// Portable collection data. Keys identify rows within an archive, never a live owner.
struct InventorySavedEntry {
    std::string key, label;
    loom::Bytes pair;
    bool capture_slot = false;
    ZEN_SHAPE(InventorySavedEntry, 1, ZEN_FIELD(key), ZEN_FIELD(label), ZEN_FIELD(pair), ZEN_FIELD(capture_slot));
};
struct InventoryArchive {
    std::vector<InventorySavedEntry> entries;
    ZEN_SHAPE(InventoryArchive, 1, ZEN_FIELD(entries));
};
struct InventorySnapshotRequested { ZEN_SHAPE(InventorySnapshotRequested, 1); };
struct InventorySnapshot {
    std::string owner;
    std::int64_t revision = 0;
    InventoryArchive archive;
    ZEN_SHAPE(InventorySnapshot, 1, ZEN_FIELD(owner), ZEN_FIELD(revision), ZEN_FIELD(archive));
};
// Compare against a fresh snapshot; replace=false also requires an empty collection.
struct InventoryRestore {
    std::string owner;
    std::int64_t revision = 0;
    InventoryArchive archive;
    bool replace = false;
    ZEN_SHAPE(InventoryRestore, 1, ZEN_FIELD(owner), ZEN_FIELD(revision), ZEN_FIELD(archive), ZEN_FIELD(replace));
};
struct InventoryRestored {
    std::string owner;
    std::int64_t entries = 0;
    ZEN_SHAPE(InventoryRestored, 1, ZEN_FIELD(owner), ZEN_FIELD(entries));
};

/// Capture into a new collection entry; existing captures and the legacy slot are retained.
struct InventoryCaptureAdd {
    std::string target_role;
    std::string label;
    ZEN_SHAPE(InventoryCaptureAdd, 1, ZEN_FIELD(target_role), ZEN_FIELD(label));
};
struct CaptureAddRequest {
    InventoryCaptureAdd request;
    ZEN_SHAPE(CaptureAddRequest, 1, ZEN_FIELD(request));
};

/// A successful capture returns its own immutable snapshot, even if a later writer replaces
/// the shared slot before the requester reads Get.
struct InventoryCaptured {
    loom::Bytes pair;
    ZEN_SHAPE(InventoryCaptured, 1, ZEN_FIELD(pair));
};

/// THE CAPTURE ADAPTER'S DOOR: ask `target_role`'s current holder to describe its own structure
/// (`zen.PokeDescribe` -- the self-description floor every woven Weave already answers,
/// unconditionally), and Set the resulting `zen.PokeStructure` as the item, with one
/// automatically derived `CaptureContext` observation and a nested `CaptureRequest` entry.
/// The answering weave is Loom-attested. Answers `InventoryCaptured` on success or `loom::Refused`
/// on a known failure. A target that never answers can leave the capture pending; no timeout
/// or completion is invented.
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

/// The actual capture request retained as structured metadata, separate from the pure item.
/// Its target is the caller's selection, not a transferable authority or a durable locator.
struct CaptureRequest {
    InventoryCaptureDescribe request;
    ZEN_SHAPE(CaptureRequest, 1, ZEN_FIELD(request));
};

} // namespace zengine::inventory

#endif // ZENGINE_INVENTORY_VOCABULARY_HPP
