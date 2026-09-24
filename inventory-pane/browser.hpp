// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_PANE_BROWSER_HPP
#define ZENGINE_INVENTORY_PANE_BROWSER_HPP
// Main Inventory's folder browser: what the last listing said, and where the maker is looking.
// Folders and membership belong to zengine.inventory; this is a presentation's copy of its
// answer plus local navigation, never reload state and never a second tree.
#include "slots.hpp"
#include "inventory/folders.hpp"
#include <map>

namespace zengine::inventory_pane {
struct Listing {
    std::string owner;
    std::int64_t revision = 0;
    std::vector<inventory::InventorySummary> entries;
    std::map<std::string, std::string> membership; // key(reference) -> folder id
    std::vector<inventory::InventoryFolderState> folders;
    inventory::FolderTree tree;
    const inventory::InventoryFolderState* folder(const std::string& id) const {
        for (const auto& f : folders) if (f.folder.folder == id) return &f;
        return nullptr;
    }
    std::string member_of(const inventory::InventoryReference& r) const {
        const auto it = membership.find(key(r));
        return it == membership.end() ? std::string{} : it->second;
    }
    std::string name(const std::string& id) const {
        const auto* f = folder(id);
        return id.empty() ? "Root" : f ? f->name : "(gone)";
    }
    /// "Root > Workbench > Samples", for notices naming an actual destination.
    std::string path(const std::string& id) const {
        std::string out = "Root";
        if (tree.contains(id)) for (const auto& at : tree.path(id)) out += " > " + name(at);
        return out;
    }
    /// Immediate subfolders, by name ignoring case, then by identity.
    std::vector<const inventory::InventoryFolderState*> children(const std::string& parent) const {
        std::vector<const inventory::InventoryFolderState*> out;
        for (const auto& f : folders) if (f.parent == parent) out.push_back(&f);
        std::sort(out.begin(), out.end(), [](const auto* a, const auto* b) {
            const auto lower = [](std::string s) { for (auto& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); return s; };
            const auto x = lower(a->name), y = lower(b->name);
            return x != y ? x < y : a->folder.folder < b->folder.folder;
        });
        return out;
    }
    /// Entries and subfolders directly inside `id`, wherever those entries are placed.
    std::size_t members(const std::string& id) const {
        std::size_t n = 0;
        for (const auto& [k, f] : membership) n += f == id;
        for (const auto& f : folders) n += f.parent == id;
        return n;
    }
};
inline Listing listing(const inventory::v2::InventoryListed& answer) {
    Listing out;
    out.owner = answer.owner; out.revision = answer.revision; out.folders = answer.folders;
    for (const auto& e : answer.entries) {
        out.entries.push_back({e.reference, e.revision, e.label, e.schema, e.version, e.capture_slot});
        out.membership[key(e.reference)] = e.folder;
    }
    std::vector<inventory::FolderTree::Row> rows;
    for (const auto& f : answer.folders) rows.push_back({f.folder.folder, f.name, f.parent});
    out.tree = inventory::FolderTree(std::move(rows));
    return out;
}

// Picture meanings for main Inventory's rows. Each carries the owner, so a restore that keeps
// folder keys still changes the picture and a queued press cannot land in the new collection.
inline std::string folder_row(const std::string& owner, const std::string& id) { return "dir:" + owner + ":" + id; }
inline std::string crumb(const std::string& owner, const std::string& id) { return "crumb:" + owner + ":" + id; }
inline constexpr const char* kUpControl = "ctl:up";
inline constexpr const char* kMoveHereControl = "ctl:here";
/// The folder a row, crumb or control means under `owner`, if it means one.
inline std::optional<std::string> folder_meant(const std::string& meaning, const std::string& owner) {
    for (const std::string prefix : {"dir:", "crumb:"})
        if (meaning.starts_with(prefix + owner + ":")) return meaning.substr(prefix.size() + owner.size() + 1);
    return std::nullopt;
}

/// Where main Inventory is looking. `path` is the folder's ancestry at the last reconcile, kept
/// so a vanished folder can fall back to its nearest surviving ancestor by identity. Seam: Back
/// and Forward history (not selected) would be one more member here; parent navigation is not
/// history.
struct Browser {
    std::string owner, folder;
    std::vector<std::string> path;
    std::map<std::string, std::string> remembered; // folder -> selected row key
    void open(const std::string& id, const Listing& now, std::string& selected) {
        remembered[folder] = selected;
        folder = id; path = now.tree.path(id);
        const auto it = remembered.find(id);
        selected = it == remembered.end() ? std::string{} : it->second;
    }
    /// Climb one level and select the folder we came from.
    bool up(const Listing& now, std::string& selected) {
        if (folder.empty()) return false;
        const auto child = folder;
        const auto* f = now.folder(folder);
        open(f ? f->parent : std::string{}, now, selected);
        selected = folder_row(now.owner, child);
        return true;
    }
    /// After a new listing: keep the folder by identity, or fall back to the nearest surviving
    /// ancestor (the root after a restore rotates the owner). Returns what changed, for a notice.
    std::string reconcile(const Listing& now, std::string& selected) {
        if (owner != now.owner) {
            const bool had = !owner.empty();
            owner = now.owner; remembered.clear();
            const bool moved = !folder.empty();
            folder.clear(); path.clear(); if (had) selected.clear();
            return had && moved ? "Inventory was replaced; browsing from Root" : "";
        }
        if (folder.empty() || now.folder(folder)) {
            const auto was = path;
            path = now.tree.path(folder);
            return was == path || folder.empty() ? "" : "This folder moved; now at " + now.path(folder);
        }
        auto fallback = std::string{};
        for (auto it = path.rbegin(); it != path.rend(); ++it) if (now.folder(*it)) { fallback = *it; break; }
        folder = fallback; path = now.tree.path(fallback);
        const auto it = remembered.find(folder);
        selected = it == remembered.end() ? std::string{} : it->second;
        return "The folder you were in is gone; now at " + now.path(folder);
    }
};
}
#endif
