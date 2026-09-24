// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_FOLDERS_HPP
#define ZENGINE_INVENTORY_FOLDERS_HPP

// Folder rules shared by the collection owner (the authority) and presentations that check a
// name before asking. docs/reference/inventory.md#named-folders states them for makers. A folder
// organizes; it is never an activation context or an authority (seam: a per-folder hotkey
// context would be presentation configuration keyed by folder identity, not a rule here).

#include <algorithm>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::inventory {

inline constexpr std::size_t kMaxFolders = 128;
inline constexpr std::size_t kMaxFolderDepth = 8; // a folder at the root has depth 1
inline constexpr std::size_t kMaxFolderName = 64;

/// Why `name` cannot name a folder, or empty. Names are stored exactly as typed; nothing trims.
inline std::string folder_name_problem(std::string_view name) {
    if (name.empty() || std::all_of(name.begin(), name.end(), [](char c) { return c == ' '; }))
        return "A folder needs a name";
    if (name.size() > kMaxFolderName ||
        std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c > 126; }))
        return "Folder names are 1 to 64 printable ASCII characters";
    if (name.find('/') != std::string_view::npos) return "Folder names cannot contain '/'";
    if (name.front() == ' ' || name.back() == ' ') return "Folder names cannot start or end with a space";
    if (name == "." || name == "..") return "'.' and '..' are not folder names";
    return {};
}

/// Sibling folders conflict when their names match ignoring ASCII case; case itself is kept.
inline bool same_folder_name(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
        const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
        return lower(x) == lower(y);
    });
}

/// A validated folder tree over (key, name, parent) rows; "" is the root and never a row.
class FolderTree {
public:
    struct Row { std::string key, name, parent; };
    FolderTree() = default;
    /// Refuses the whole candidate: bounds, names, duplicate keys, missing parents, cycles,
    /// depth and sibling conflicts are all checked before anything is kept.
    explicit FolderTree(std::vector<Row> rows) {
        if (rows.size() > kMaxFolders) throw std::invalid_argument("at most 128 folders");
        for (auto& row : rows) {
            if (row.key.empty() || !nodes_.emplace(row.key, row).second)
                throw std::invalid_argument("folders have an empty or duplicate identity");
            if (const auto problem = folder_name_problem(row.name); !problem.empty())
                throw std::invalid_argument(problem);
        }
        for (const auto& [key, row] : nodes_)
            if (!row.parent.empty() && !nodes_.contains(row.parent))
                throw std::invalid_argument("a folder's parent is missing");
        for (const auto& [key, row] : nodes_) {
            std::size_t steps = 0;
            for (auto at = row.parent; !at.empty(); at = nodes_.at(at).parent)
                if (at == key || ++steps > nodes_.size()) throw std::invalid_argument("a folder is inside itself");
            (void)depth(key);
            for (const auto& [other, sibling] : nodes_)
                if (other != key && sibling.parent == row.parent && same_folder_name(sibling.name, row.name))
                    throw std::invalid_argument("two folders in one place share the name '" + row.name + "'");
        }
    }
    bool contains(const std::string& key) const { return key.empty() || nodes_.contains(key); }
    const Row* find(const std::string& key) const {
        const auto it = nodes_.find(key);
        return it == nodes_.end() ? nullptr : &it->second;
    }
    std::size_t size() const { return nodes_.size(); }
    /// Root is 0. Throws on a cycle or a depth past the bound.
    std::size_t depth(const std::string& key) const {
        std::size_t depth = 0;
        for (auto at = key; !at.empty(); at = nodes_.at(at).parent)
            if (++depth > kMaxFolderDepth) throw std::invalid_argument("folders nest at most eight deep");
        return depth;
    }
    /// Is `key` the folder `ancestor` or inside it?
    bool within(const std::string& key, const std::string& ancestor) const {
        if (ancestor.empty()) return true;
        for (auto at = key; !at.empty(); at = nodes_.at(at).parent)
            if (at == ancestor) return true;
        return false;
    }
    /// Levels below `key`, 0 when it has no subfolders.
    std::size_t height(const std::string& key) const {
        std::size_t most = 0;
        for (const auto& [other, row] : nodes_)
            if (other != key && within(other, key)) most = std::max(most, depth(other) - depth(key));
        return most;
    }
    /// Is `name` taken by a folder in `parent` other than `except`?
    bool taken(const std::string& parent, std::string_view name, const std::string& except = {}) const {
        return std::any_of(nodes_.begin(), nodes_.end(), [&](const auto& node) {
            return node.first != except && node.second.parent == parent && same_folder_name(node.second.name, name);
        });
    }
    std::vector<std::string> children(const std::string& parent) const {
        std::vector<std::string> out;
        for (const auto& [key, row] : nodes_) if (row.parent == parent) out.push_back(key);
        return out;
    }
    /// Keys from the top-level ancestor down to `key`; empty for the root.
    std::vector<std::string> path(const std::string& key) const {
        std::vector<std::string> out;
        for (auto at = key; !at.empty(); at = nodes_.at(at).parent) out.insert(out.begin(), at);
        return out;
    }

private:
    std::map<std::string, Row> nodes_;
};

} // namespace zengine::inventory

#endif // ZENGINE_INVENTORY_FOLDERS_HPP
