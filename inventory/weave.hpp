// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_WEAVE_HPP
#define ZENGINE_INVENTORY_WEAVE_HPP

// One loadable inventory: complete typed pairs are owned as independent bytes.
// CaptureDescribe is a narrow acquisition adapter; Set/Get stay schema-generic.
// See docs/reference/inventory.md for authority, pending behavior and lifetime.

#include "inventory/codec.hpp"
#include "inventory/archive.hpp"
#include "inventory/folders.hpp"
#include "inventory/grant.hpp"
#include "inventory/observation.hpp"
#include "inventory/vocabulary.hpp"
#include "activation/activation.hpp"

#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/dispatch_refusal.hpp>
#include <zen/weave/poke.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <chrono>
#include <algorithm>
#include <cstdint>
#include <exception>
#include <optional>
#include <limits>
#include <random>
#include <string>
#include <utility>

namespace zengine::inventory {

// Membership (`folder`, empty at the root) is organization beside the label: changing it never
// changes the entry's identity, revision or pair.
struct StoredEntry {
    std::string entry;
    std::int64_t revision = 1;
    loom::Bytes pair;
    std::string label;
    std::string folder;
    ZEN_SHAPE(StoredEntry, 2, ZEN_FIELD(entry), ZEN_FIELD(revision), ZEN_FIELD(pair), ZEN_FIELD(label),
              ZEN_FIELD(folder));
};
struct StoredFolder {
    std::string folder;
    std::int64_t revision = 1;
    std::string name;
    std::string parent;
    ZEN_SHAPE(StoredFolder, 1, ZEN_FIELD(folder), ZEN_FIELD(revision), ZEN_FIELD(name), ZEN_FIELD(parent));
};
struct InventoryWeaveState {
    bool occupied = false;
    loom::Bytes pair;
    std::string entry;
    std::int64_t revision = 0;
    std::string label;
    std::vector<StoredEntry> entries;
    std::string folder; // the legacy slot's folder; a replacement starts at the root
    std::vector<StoredFolder> folders;
    ZEN_SHAPE(InventoryWeaveState, 4, ZEN_FIELD(occupied), ZEN_FIELD(pair), ZEN_FIELD(entry),
              ZEN_FIELD(revision), ZEN_FIELD(label), ZEN_FIELD(entries), ZEN_FIELD(folder),
              ZEN_FIELD(folders));
};

class InventoryWeave final
    : public loom::WeaveBase<
          InventoryWeave, InventoryWeaveState,
          loom::Accept<loom::Activated, InventorySet, InventoryGet, InventoryLocate, InventoryRead, InventoryWrite,
                       InventoryAdd, v2::InventoryAdd, InventoryList, v2::InventoryList,
                       InventoryRename, InventoryRemove,
                       InventoryFolderCreate, InventoryFolderRename, InventoryFolderMove,
                       InventoryFolderRemove, InventoryFile,
                       InventorySnapshotRequested, v2::InventorySnapshotRequested,
                       InventoryRestore, v2::InventoryRestore,
                       InventoryCaptureAdd, InventoryCaptureDescribe, loom::PokeStructure,
                       loom::DispatchRefused>,
          loom::Emit<InventoryState, InventoryEntry, InventoryListed, v2::InventoryListed,
                     InventoryFolderState, InventoryChanged,
                     InventorySnapshot, v2::InventorySnapshot, InventoryRestored,
                     InventoryCaptured, loom::Ack, loom::Refused,
                     loom::PokeDescribe>> {
public:
    InventoryWeave() : book_(1) {}
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (activation_.accept(mail, a)) changed(mail);
    }

    // ---- the generic, schema-opaque doors -------------------------------------------------

    /// Replace the slot, or refuse whole. `decode_pair` is asked to validate `req.pair`
    /// completely -- malformed bytes, a closure that disagrees with itself, a value that does
    /// not admit against its own declared root -- before a single byte of the previous pair is
    /// touched; its decoded result is otherwise unused here; the stored form stays the caller's
    /// own bytes, which the next Get hands back exactly, rather than a re-encoding of what this
    /// weave happened to notice while checking.
    void on(const InventorySet& req, loom::Mail& mail) {
        try {
            (void)decode_pair(
                std::string_view(reinterpret_cast<const char*>(req.pair.data()), req.pair.size()));
            replace(req.pair);
        } catch (const std::exception& e) {
            (void)mail.answer(loom::Refused{std::string("Set refused: ") + e.what()});
            return;
        }
        (void)mail.answer(loom::Ack{});
        changed(mail);
    }

    /// Always answered: `occupied=false, pair=[]` before the first successful Set is the
    /// understandable empty result, not a refusal. `out.pair` is a value-copy of `state_.pair`,
    /// and the reply crosses the bus as its own admitted Value -- independently readable after a
    /// later Set replaces the slot, and unable to mutate this weave's storage back.
    void on(const InventoryGet&, loom::Mail& mail) {
        InventoryState out;
        out.occupied = state_.occupied;
        out.pair = state_.pair;
        (void)mail.answer(out);
    }

    void on(const InventoryLocate&, loom::Mail& mail) {
        if (!state_.occupied) {
            (void)mail.answer(loom::Refused{"the inventory is empty"});
            return;
        }
        (void)mail.answer(entry_snapshot());
    }

    void on(const InventoryRead& req, loom::Mail& mail) {
        if (const auto* entry = find(req.reference)) {
            (void)mail.answer(stored_snapshot(*entry));
            return;
        }
        if (!matches(req.reference)) {
            (void)mail.answer(loom::Refused{"this inventory entry is no longer here"});
            return;
        }
        (void)mail.answer(entry_snapshot());
    }

    void on(const InventoryWrite& req, loom::Mail& mail) {
        if (auto* entry = find(req.reference)) {
            if (!check_revision(entry->revision, req.revision, mail)) return;
            try {
                (void)decode_pair(view(req.pair));
                loom::Bytes replacement = req.pair;
                entry->pair.swap(replacement);
                ++entry->revision;
            } catch (const std::exception& e) {
                (void)mail.answer(loom::Refused{std::string("save refused: ") + e.what()});
                return;
            }
            (void)mail.answer(stored_snapshot(*entry)); changed(mail); return;
        }
        if (!matches(req.reference)) {
            (void)mail.answer(loom::Refused{"this inventory entry is no longer here"});
            return;
        }
        if (req.revision != state_.revision) {
            (void)mail.answer(loom::Refused{"the entry changed; fetch a fresh copy before saving"});
            return;
        }
        if (state_.revision == std::numeric_limits<std::int64_t>::max()) {
            (void)mail.answer(loom::Refused{"this entry's revision counter is exhausted"});
            return;
        }
        try {
            (void)decode_pair(std::string_view(reinterpret_cast<const char*>(req.pair.data()),
                                               req.pair.size()));
            loom::Bytes replacement = req.pair;
            state_.pair.swap(replacement);
            ++state_.revision;
        } catch (const std::exception& e) {
            (void)mail.answer(loom::Refused{std::string("save refused: ") + e.what()});
            return;
        }
        (void)mail.answer(entry_snapshot());
        changed(mail);
    }

    void on(const InventoryAdd& req, loom::Mail& mail) {
        try { (void)mail.answer(add(req.pair, req.label)); changed(mail); }
        catch (const std::exception& e) { (void)mail.answer(loom::Refused{e.what()}); }
    }
    void on(const v2::InventoryAdd& req, loom::Mail& mail) {
        try {
            if (!folder_here(req.folder)) throw std::invalid_argument("that folder is no longer here");
            (void)mail.answer(add(req.pair, req.label, req.folder.folder)); changed(mail);
        } catch (const std::exception& e) { (void)mail.answer(loom::Refused{e.what()}); }
    }
    void on(const v2::InventoryList&, loom::Mail& mail) {
        v2::InventoryListed out{owner_identity_, collection_revision_, {}, {}};
        const auto append = [&](const InventoryEntry& entry, const std::string& label, bool slot,
                                const std::string& folder) {
            const auto pair = decode_pair(view(entry.pair));
            const auto& schema = pair.item.schema();
            out.entries.push_back({entry.reference, entry.revision,
                label.empty() ? schema.name() : label, schema.name(), schema.version(), slot, folder});
        };
        if (state_.occupied) append(entry_snapshot(), state_.label, true, state_.folder);
        for (const auto& entry : state_.entries) append(stored_snapshot(entry), entry.label, false, entry.folder);
        for (const auto& folder : state_.folders) out.folders.push_back(folder_snapshot(folder));
        (void)mail.answer(out);
    }

    // ---- named folders: organization of the collection, owned here beside the labels ------

    void on(const InventoryFolderCreate& req, loom::Mail& mail) {
        try {
            const auto tree = folder_tree();
            if (!folder_here(req.parent)) throw std::invalid_argument("that folder is no longer here");
            if (const auto problem = folder_name_problem(req.name); !problem.empty())
                throw std::invalid_argument(problem);
            if (state_.folders.size() >= kMaxFolders) throw std::invalid_argument("Inventory holds at most 128 folders");
            if (tree.depth(req.parent.folder) + 1 > kMaxFolderDepth)
                throw std::invalid_argument("folders nest at most eight deep");
            if (tree.taken(req.parent.folder, req.name))
                throw std::invalid_argument("'" + req.name + "' is already a folder here");
            StoredFolder row{new_identity(), 1, req.name, req.parent.folder};
            const auto answer = folder_snapshot(row);
            state_.folders.push_back(std::move(row));
            (void)mail.answer(answer); changed(mail);
        } catch (const std::exception& e) { (void)mail.answer(loom::Refused{e.what()}); }
    }
    void on(const InventoryFolderRename& req, loom::Mail& mail) {
        try {
            auto* folder = current_folder(req.folder, req.revision);
            if (const auto problem = folder_name_problem(req.name); !problem.empty())
                throw std::invalid_argument(problem);
            if (folder_tree().taken(folder->parent, req.name, folder->folder))
                throw std::invalid_argument("'" + req.name + "' is already a folder there");
            std::string name = req.name;
            folder->name.swap(name); ++folder->revision;
            (void)mail.answer(folder_snapshot(*folder)); changed(mail);
        } catch (const std::exception& e) { (void)mail.answer(loom::Refused{e.what()}); }
    }
    void on(const InventoryFolderMove& req, loom::Mail& mail) {
        try {
            auto* folder = current_folder(req.folder, req.revision);
            const auto tree = folder_tree();
            if (!folder_here(req.into)) throw std::invalid_argument("the destination folder is no longer here");
            if (tree.within(req.into.folder, folder->folder))
                throw std::invalid_argument("a folder cannot move into itself or a folder inside it");
            if (req.into.folder == folder->parent) { (void)mail.answer(folder_snapshot(*folder)); return; }
            if (tree.depth(req.into.folder) + 1 + tree.height(folder->folder) > kMaxFolderDepth)
                throw std::invalid_argument("that would nest folders more than eight deep");
            if (tree.taken(req.into.folder, folder->name, folder->folder))
                throw std::invalid_argument("'" + folder->name + "' is already a folder there");
            folder->parent = req.into.folder; ++folder->revision;
            (void)mail.answer(folder_snapshot(*folder)); changed(mail);
        } catch (const std::exception& e) { (void)mail.answer(loom::Refused{e.what()}); }
    }
    void on(const InventoryFolderRemove& req, loom::Mail& mail) {
        try {
            const auto* folder = current_folder(req.folder, req.revision, false);
            std::size_t entries = state_.occupied && state_.folder == folder->folder ? 1 : 0, folders = 0;
            for (const auto& row : state_.entries) entries += row.folder == folder->folder;
            for (const auto& row : state_.folders) folders += row.parent == folder->folder;
            if (entries || folders)
                throw std::invalid_argument("'" + folder->name + "' still holds " + std::to_string(entries) +
                    " entr" + (entries == 1 ? "y" : "ies") + " and " + std::to_string(folders) + " folder" +
                    (folders == 1 ? "" : "s") + "; move them out first. Removing a folder never removes entries");
            const auto id = folder->folder;
            std::erase_if(state_.folders, [&](const auto& row) { return row.folder == id; });
            (void)mail.answer(loom::Ack{}); changed(mail);
        } catch (const std::exception& e) { (void)mail.answer(loom::Refused{e.what()}); }
    }
    void on(const InventoryFile& req, loom::Mail& mail) {
        auto* entry = find(req.reference);
        if (!entry && !matches(req.reference)) { missing(mail); return; }
        auto& folder = entry ? entry->folder : state_.folder;
        if (req.from.owner != owner_identity_ || req.from.folder != folder) {
            (void)mail.answer(loom::Refused{"the entry is no longer in that folder; look again"});
            return;
        }
        if (!folder_here(req.into)) {
            (void)mail.answer(loom::Refused{"the destination folder is no longer here"});
            return;
        }
        const bool moved = folder != req.into.folder;
        folder = req.into.folder;
        (void)mail.answer(entry ? stored_snapshot(*entry) : entry_snapshot());
        if (moved) changed(mail);
    }
    void on(const InventoryList&, loom::Mail& mail) {
        InventoryListed out;
        const auto append = [&](const InventoryEntry& entry, const std::string& label, bool slot) {
            const auto pair = decode_pair(view(entry.pair));
            const auto& schema = pair.item.schema();
            out.entries.push_back({entry.reference, entry.revision,
                label.empty() ? schema.name() : label, schema.name(), schema.version(), slot});
        };
        if (state_.occupied) append(entry_snapshot(), state_.label, true);
        for (const auto& entry : state_.entries) append(stored_snapshot(entry), entry.label, false);
        (void)mail.answer(out);
    }
    void on(const InventoryRename& req, loom::Mail& mail) {
        auto* entry = find(req.reference);
        if (!entry && !matches(req.reference)) { missing(mail); return; }
        auto& revision = entry ? entry->revision : state_.revision;
        if (!check_revision(revision, req.revision, mail)) return;
        try {
            validate_label(req.label);
            std::string label = req.label;
            (entry ? entry->label : state_.label).swap(label);
            ++revision;
            (void)mail.answer(entry ? stored_snapshot(*entry) : entry_snapshot()); changed(mail);
        } catch (const std::exception& e) { (void)mail.answer(loom::Refused{e.what()}); }
    }
    void on(const InventoryRemove& req, loom::Mail& mail) {
        auto* entry = find(req.reference);
        if (!entry && !matches(req.reference)) { missing(mail); return; }
        if (!check_revision(entry ? entry->revision : state_.revision, req.revision, mail, false)) return;
        if (entry) {
            state_.entries.erase(std::remove_if(state_.entries.begin(), state_.entries.end(),
                [&](const auto& row) { return row.entry == req.reference.entry; }), state_.entries.end());
        } else {
            state_.occupied = false; state_.pair.clear(); state_.entry.clear(); state_.label.clear();
            state_.folder.clear();
        }
        (void)mail.answer(loom::Ack{}); changed(mail);
    }

    /// Version 1 cannot say where anything is filed, so it refuses rather than drop folders.
    void on(const InventorySnapshotRequested&, loom::Mail& mail) {
        const bool filed = !state_.folders.empty() || (state_.occupied && !state_.folder.empty()) ||
            std::any_of(state_.entries.begin(), state_.entries.end(), [](const auto& row) { return !row.folder.empty(); });
        if (filed) {
            (void)mail.answer(loom::Refused{"this inventory has folders; ask for a version 2 snapshot"});
            return;
        }
        InventorySnapshot out{owner_identity_, collection_revision_, {}};
        if (state_.occupied) out.archive.entries.push_back({state_.entry, state_.label, state_.pair, true});
        for (const auto& row : state_.entries)
            out.archive.entries.push_back({row.entry, row.label, row.pair, false});
        try {
            validate_archive(out.archive);
            (void)mail.answer(out);
        } catch (const std::exception& e) { (void)mail.answer(loom::Refused{e.what()}); }
    }
    void on(const v2::InventorySnapshotRequested&, loom::Mail& mail) {
        v2::InventorySnapshot out{owner_identity_, collection_revision_, {}};
        if (state_.occupied)
            out.archive.entries.push_back({state_.entry, state_.label, state_.pair, true, state_.folder});
        for (const auto& row : state_.entries)
            out.archive.entries.push_back({row.entry, row.label, row.pair, false, row.folder});
        for (const auto& row : state_.folders) out.archive.folders.push_back({row.folder, row.name, row.parent});
        try {
            validate_archive(out.archive);
            (void)mail.answer(out);
        } catch (const std::exception& e) { (void)mail.answer(loom::Refused{e.what()}); }
    }

    /// Version 1 archives are flat: every restored entry is at the root.
    void on(const InventoryRestore& req, loom::Mail& mail) {
        restore(req.owner, req.revision, organized(req.archive), req.replace, mail);
    }
    void on(const v2::InventoryRestore& req, loom::Mail& mail) {
        restore(req.owner, req.revision, req.archive, req.replace, mail);
    }
    void restore(const std::string& owner, std::int64_t revision, const v2::InventoryArchive& archive,
                 bool replace, loom::Mail& mail) {
        try {
            if (book_.awaiting()) throw std::invalid_argument("finish the pending capture before restoring a toolbox");
            if (owner != owner_identity_ || revision != collection_revision_ ||
                collection_revision_ == std::numeric_limits<std::int64_t>::max())
                throw std::invalid_argument("inventory changed while preparing restore; request restore again");
            if (!replace && (state_.occupied || !state_.entries.empty() || !state_.folders.empty()))
                throw std::invalid_argument("inventory is not empty; explicitly choose replace to restore this toolbox");
            validate_archive(archive);
            InventoryWeaveState candidate;
            for (const auto& row : archive.entries) {
                if (row.capture_slot) {
                    candidate.occupied = true; candidate.entry = row.key; candidate.folder = row.folder;
                    candidate.label = row.label; candidate.pair = row.pair; candidate.revision = 1;
                } else candidate.entries.push_back({row.key, 1, row.pair, row.label, row.folder});
            }
            for (const auto& row : archive.folders) candidate.folders.push_back({row.key, 1, row.name, row.parent});
            auto fresh = new_identity();
            // Allocate and validate everything before replacing the authoritative collection.
            InventoryRestored answer{fresh, static_cast<std::int64_t>(archive.entries.size())};
            state_ = std::move(candidate);
            owner_identity_.swap(fresh); collection_revision_ = 0;
            (void)mail.answer(answer); changed(mail);
        } catch (const std::exception& e) { (void)mail.answer(loom::Refused{e.what()}); }
    }

    // ---- the capture adapter's door -------------------------------------------------------

    void on(const InventoryCaptureDescribe& req, loom::Mail& mail) {
        capture(req.target_role, {}, false, mail);
    }
    void on(const InventoryCaptureAdd& req, loom::Mail& mail) {
        capture(req.target_role, req.label, true, mail);
    }
    void capture(const std::string& target_role, const std::string& label, bool append, loom::Mail& mail) {
        if (book_.awaiting()) {
            (void)mail.answer(loom::Refused{"a capture is already in flight for this inventory"});
            return;
        }
        if (target_role.empty()) {
            (void)mail.answer(loom::Refused{"Inventory capture needs a target_role"});
            return;
        }
        try { validate_label(label); }
        catch (const std::exception& e) { (void)mail.answer(loom::Refused{e.what()}); return; }
        loom::DeferredAnswer due = mail.defer_answer();
        if (!due.valid()) {
            (void)mail.answer(loom::Refused{"this delivery cannot defer its answer"});
            return;
        }
        pending_answer_ = std::move(due);
        requested_role_ = target_role;
        capture_label_ = label;
        capture_append_ = append;
        const loom::AskOpened opened = book_.open_to_role(
            target_role, loom::PokeDescribe::zen_name, loom::PokeDescribe::zen_version);
        if (!opened.ok) {
            finish(mail, loom::Refused{"this inventory's own book is full"});
            return;
        }
        const auto sent = mail.send_to_role(target_role, loom::PokeDescribe{}, opened.correlation);
        if (!sent.valid() || !book_.bind_attempt(opened.id, sent.seq)) {
            (void)book_.forget(opened.id);
            finish(mail, loom::Refused{"capture request could not be queued"});
        }
    }

    /// THE OBSERVED ANSWER. `structure` is a fresh struct this delivery decoded from its own
    /// gated Value -- independent of whatever `target_role`'s holder keeps -- and `mail.sender()`
    /// is Loom's own stamp, never something the reply's payload could claim to be.
    void on(const loom::PokeStructure& structure, loom::Mail& mail) {
        const std::optional<loom::PendingAsk> asked = settled(mail);
        if (!asked.has_value()) {
            return; // not this weave's own open conversation: data, and moves nothing
        }
        const CaptureContext ctx = observed_structure(requested_role_, mail.sender());
        InventoryCaptured result;
        loom::Bytes stored;
        try {
            const std::string encoded = encode_pair(loom::to_value(structure), {
                loom::to_value(ctx), capture_append_
                    ? loom::to_value(CaptureAddRequest{{requested_role_, capture_label_}})
                    : loom::to_value(CaptureRequest{{requested_role_}})});
            stored.assign(encoded.begin(), encoded.end());
            if (capture_append_) {
                const auto added = add(std::move(stored), capture_label_);
                finish(mail, added); changed(mail); return;
            }
            result.pair = stored;
            replace(std::move(stored));
        } catch (const std::exception& e) {
            finish(mail, loom::Refused{std::string("capture could not be stored: ") + e.what()});
            return;
        }
        finish(mail, result);
        changed(mail);
    }

    // A notice is Loom's dispatch fact only with provenance AND this send's identity.
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!mail.dispatch_refused() || !pending_answer_.valid()) {
            return;
        }
        const auto* asked = book_.match_attempt(refused.refused_attempt().seq);
        if (asked == nullptr || refused.role != requested_role_ || !refused.target.empty() ||
            refused.shape != loom::PokeDescribe::zen_name ||
            refused.version != loom::PokeDescribe::zen_version) {
            return;
        }
        (void)book_.forget(asked->id);
        finish(mail, loom::Refused{"'" + requested_role_ +
                                   "' could not be asked zen.PokeDescribe: " + refused.reason});
    }

private:
    static std::string_view view(const loom::Bytes& bytes) {
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }
    static void validate_label(const std::string& label) {
        if (label.size() > 80 || std::any_of(label.begin(), label.end(),
            [](unsigned char c) { return c < 32 || c > 126; }))
            throw std::invalid_argument("entry names must be at most 80 printable ASCII characters");
    }
    static bool check_revision(std::int64_t current, std::int64_t wanted, loom::Mail& mail,
                               bool increment = true) {
        if (current != wanted || (increment && current == std::numeric_limits<std::int64_t>::max())) {
            (void)mail.answer(loom::Refused{"the entry changed or its revision is exhausted; fetch a fresh copy"});
            return false;
        }
        return true;
    }
    static void missing(loom::Mail& mail) {
        (void)mail.answer(loom::Refused{"this inventory entry is no longer here"});
    }
    void changed(loom::Mail& mail) {
        if (collection_revision_ < std::numeric_limits<std::int64_t>::max()) ++collection_revision_;
        (void)mail.as_role(kInventoryRole).publish(InventoryChanged{});
    }
    StoredEntry* find(const InventoryReference& ref) {
        if (ref.owner != owner_identity_) return nullptr;
        for (auto& row : state_.entries) if (row.entry == ref.entry) return &row;
        return nullptr;
    }
    InventoryEntry stored_snapshot(const StoredEntry& row) const {
        return {{owner_identity_, row.entry}, row.revision, row.pair};
    }
    InventoryEntry add(loom::Bytes pair, std::string label, std::string folder = {}) {
        if (state_.entries.size() >= 256) throw std::invalid_argument("inventory has reached its 256 saved-entry limit");
        (void)decode_pair(view(pair)); validate_label(label);
        StoredEntry row{new_identity(), 1, std::move(pair), std::move(label), std::move(folder)};
        auto answer = stored_snapshot(row);
        state_.entries.push_back(std::move(row));
        return answer;
    }
    // The nonce distinguishes replacements even when the bytes match. It is not a secret
    // capability. Build the entire replacement first so allocation/entropy failure is atomic.
    // A replacement is a new entry at the root: it never inherits the old occupant's folder.
    void replace(loom::Bytes pair) {
        std::string entry = new_identity();
        state_.entry.swap(entry);
        state_.pair.swap(pair);
        state_.revision = 1;
        state_.occupied = true;
        state_.label.clear();
        state_.folder.clear();
    }
    FolderTree folder_tree() const {
        std::vector<FolderTree::Row> rows;
        for (const auto& row : state_.folders) rows.push_back({row.folder, row.name, row.parent});
        return FolderTree(std::move(rows));
    }
    /// The root or a folder of this owner's current collection.
    bool folder_here(const InventoryFolderReference& ref) const {
        return ref.owner == owner_identity_ && (ref.folder.empty() ||
            std::any_of(state_.folders.begin(), state_.folders.end(), [&](const auto& row) { return row.folder == ref.folder; }));
    }
    StoredFolder* current_folder(const InventoryFolderReference& ref, std::int64_t revision, bool increment = true) {
        StoredFolder* found = nullptr;
        if (ref.owner == owner_identity_ && !ref.folder.empty())
            for (auto& row : state_.folders) if (row.folder == ref.folder) found = &row;
        if (!found) throw std::invalid_argument("that folder is no longer here");
        if (found->revision != revision || (increment && revision == std::numeric_limits<std::int64_t>::max()))
            throw std::invalid_argument("the folder changed; look again before changing it");
        return found;
    }
    InventoryFolderState folder_snapshot(const StoredFolder& row) const {
        return {{owner_identity_, row.folder}, row.revision, row.name, row.parent};
    }

    static std::string new_identity() {
        std::random_device entropy;
        constexpr char digits[] = "0123456789abcdef";
        std::string entry;
        entry.reserve(32);
        for (int i = 0; i < 16; ++i) {
            const auto byte = static_cast<unsigned>(entropy()) & 255u;
            entry.push_back(digits[byte >> 4u]);
            entry.push_back(digits[byte & 15u]);
        }
        return entry;
    }

    bool matches(const InventoryReference& ref) const {
        return state_.occupied && ref.owner == owner_identity_ && ref.entry == state_.entry;
    }

    InventoryEntry entry_snapshot() const {
        return {{owner_identity_, state_.entry}, state_.revision, state_.pair};
    }

    // Role asks cannot pre-bind a respondent. AskBook matches bookkeeping; Loom's
    // answer provenance proves that the routed request earned this answer.
    std::optional<loom::PendingAsk> settled(loom::Mail& mail) {
        if (!pending_answer_.valid() || !mail.answers_ask()) {
            return std::nullopt;
        }
        return book_.settle(mail.correlation(), mail.sender());
    }

    template <class T>
    void finish(loom::Mail& mail, const T& answer) {
        loom::DeferredAnswer due = std::move(pending_answer_);
        pending_answer_ = loom::DeferredAnswer{};
        requested_role_.clear();
        (void)loom::answer_deferred(due, mail, answer);
    }

    std::string owner_identity_ = new_identity();
    std::int64_t collection_revision_ = 0;
    zengine::ActivationCursor activation_;
    loom::AskBook book_;
    loom::DeferredAnswer pending_answer_;
    std::string requested_role_;
    std::string capture_label_;
    bool capture_append_ = false;
};

} // namespace zengine::inventory

#endif // ZENGINE_INVENTORY_WEAVE_HPP
