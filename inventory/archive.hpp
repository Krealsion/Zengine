// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_ARCHIVE_HPP
#define ZENGINE_INVENTORY_ARCHIVE_HPP
#include "codec.hpp"
#include "folders.hpp"
#include "vocabulary.hpp"
#include <algorithm>
#include <set>

namespace zengine::inventory {
inline constexpr std::size_t kMaxArchivePairBytes = 8u << 20;
inline bool archive_key(const std::string& key) {
    return key.size() == 32 && std::all_of(key.begin(), key.end(),
        [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
/// The whole candidate or nothing: entry rows, then the folder tree and every membership.
inline void validate_archive(const v2::InventoryArchive& archive) {
    if (archive.entries.size() > 257) throw std::invalid_argument("toolbox exceeds the inventory capacity");
    std::set<std::string> keys;
    std::size_t bytes = 0, saved = 0, captures = 0;
    for (const auto& row : archive.entries) {
        if (!archive_key(row.key) || !keys.insert(row.key).second)
            throw std::invalid_argument("toolbox has an invalid or duplicate entry key");
        if (row.label.size() > 80 || std::any_of(row.label.begin(), row.label.end(),
                [](unsigned char c) { return c < 32 || c > 126; }))
            throw std::invalid_argument("toolbox entry names need at most 80 printable ASCII characters");
        if (row.pair.size() > kMaxArchivePairBytes - bytes)
            throw std::invalid_argument("toolbox pairs exceed the 8 MiB limit");
        bytes += row.pair.size();
        row.capture_slot ? ++captures : ++saved;
        if (captures > 1 || saved > 256) throw std::invalid_argument("toolbox exceeds the inventory capacity");
        (void)decode_pair({reinterpret_cast<const char*>(row.pair.data()), row.pair.size()});
    }
    std::vector<FolderTree::Row> rows;
    for (const auto& folder : archive.folders) {
        if (!archive_key(folder.key) || keys.contains(folder.key))
            throw std::invalid_argument("toolbox has an invalid folder key");
        rows.push_back({folder.key, folder.name, folder.parent});
    }
    const FolderTree tree(std::move(rows));
    for (const auto& row : archive.entries)
        if (!tree.contains(row.folder)) throw std::invalid_argument("a toolbox entry names a missing folder");
}
/// A version 1 archive is flat: read it with every entry at the root and no folders.
inline v2::InventoryArchive organized(const InventoryArchive& flat) {
    v2::InventoryArchive out;
    for (const auto& row : flat.entries) out.entries.push_back({row.key, row.label, row.pair, row.capture_slot, {}});
    return out;
}
inline void validate_archive(const InventoryArchive& archive) { validate_archive(organized(archive)); }
}
#endif
