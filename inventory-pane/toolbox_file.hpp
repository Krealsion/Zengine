// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_PANE_TOOLBOX_FILE_HPP
#define ZENGINE_INVENTORY_PANE_TOOLBOX_FILE_HPP
#include "slots.hpp"
#include "inventory/archive.hpp"
#include "maker/files.hpp"
#include "workshop/keymap.hpp"
#include <array>
#include <charconv>
#include <limits>

namespace zengine::inventory_pane {
// File-only configuration has no live owner, active flag, actor or pending operation.
struct SavedView {
    std::string id, kind;
    std::vector<std::string> entries;
    ZEN_SHAPE(SavedView, 1, ZEN_FIELD(id), ZEN_FIELD(kind), ZEN_FIELD(entries));
};
struct SavedBinding {
    std::string entry, target;
    std::int64_t scancode = 0, modifiers = 0;
    ZEN_SHAPE(SavedBinding, 1, ZEN_FIELD(entry), ZEN_FIELD(target), ZEN_FIELD(scancode), ZEN_FIELD(modifiers));
};
struct InventoryToolbox {
    inventory::InventoryArchive archive;
    std::vector<SavedView> views;
    std::vector<SavedBinding> bindings;
    ZEN_SHAPE(InventoryToolbox, 1, ZEN_FIELD(archive), ZEN_FIELD(views), ZEN_FIELD(bindings));
};
inline constexpr std::size_t kMaxToolboxFileBytes = 32u << 20;

inline std::int64_t toolbox_view_number(const std::string& id) {
    if (!id.starts_with("inventory.") || id.size() <= 10 || id.size() > 29)
        throw std::invalid_argument("toolbox has an invalid view identity");
    std::int64_t number = 0;
    const auto parsed = std::from_chars(id.data() + 10, id.data() + id.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != id.data() + id.size() || number <= 0 ||
        number > std::numeric_limits<std::int64_t>::max() - 32 ||
        id != "inventory." + std::to_string(number))
        throw std::invalid_argument("toolbox has an invalid view identity");
    return number;
}

inline void validate_toolbox(const InventoryToolbox& file) {
    inventory::validate_archive(file.archive);
    if (file.views.size() > 12 || file.bindings.size() > 16)
        throw std::invalid_argument("toolbox exceeds the portable view or binding limit");
    std::set<std::string> entries, placed, bound, views;
    for (const auto& e : file.archive.entries) entries.insert(e.key);
    for (const auto& v : file.views) {
        (void)toolbox_view_number(v.id);
        if (!views.insert(v.id).second) throw std::invalid_argument("toolbox has duplicate view identities");
        if ((v.kind != "single" && v.kind != "row" && v.kind != "column") ||
            (v.kind == "single" && v.entries.size() > 1))
            throw std::invalid_argument("toolbox has an invalid portable view");
        for (const auto& id : v.entries)
            if (!entries.contains(id) || !placed.insert(id).second)
                throw std::invalid_argument("toolbox placement is missing or duplicated");
    }
    for (const auto& b : file.bindings) {
        if (!entries.contains(b.entry) || !bound.insert(b.entry).second || b.target.empty() ||
            b.target.size() > 128 || std::any_of(b.target.begin(), b.target.end(),
                [](unsigned char c) { return c <= 32 || c > 126; }) ||
            workshop::key_name_of(b.scancode) == nullptr || !b.scancode ||
            (b.modifiers & ~(input::mod::kCtrl | input::mod::kShift | input::mod::kAlt | input::mod::kSuper)))
            throw std::invalid_argument("toolbox has an invalid entry binding");
    }
}

inline InventoryToolbox toolbox_snapshot(const inventory::InventorySnapshot& snapshot,
                                         const InventoryViews& layout) {
    InventoryToolbox file{snapshot.archive, {}, {}};
    const auto check_owner = [&](const inventory::InventoryReference& ref) {
        if (ref.owner != snapshot.owner)
            throw std::invalid_argument("a configured entry belongs to an unavailable inventory; resolve it before saving");
    };
    for (const auto& view : layout.views) {
        SavedView saved{view.id, view.kind, {}};
        for (const auto& ref : view.entries) { check_owner(ref); saved.entries.push_back(ref.entry); }
        file.views.push_back(std::move(saved));
    }
    for (const auto& b : layout.bindings) {
        check_owner(b.reference);
        file.bindings.push_back({b.reference.entry, b.target, b.scancode, b.modifiers});
    }
    validate_toolbox(file);
    return file;
}

inline InventoryViews toolbox_layout(const InventoryToolbox& file, const InventoryViews& previous) {
    validate_toolbox(file);
    InventoryViews layout;
    layout.serial = previous.serial;
    if (layout.serial < 0 || layout.serial > std::numeric_limits<std::int64_t>::max() - 32)
        throw std::invalid_argument("portable view identity counter is exhausted");
    // Offers have no withdrawal door. Reuse their identities; unused offered views become
    // empty, inactive spares. Repeated restores never accumulate unbounded pane identities.
    for (const auto& v : previous.views) layout.views.push_back({v.id, v.kind, false, {}});
    for (const auto& saved : file.views) {
        layout.serial = std::max(layout.serial, toolbox_view_number(saved.id));
        auto* view = find(layout, saved.id);
        if (!view) {
            if (layout.views.size() >= 12)
                throw std::invalid_argument("toolbox plus already offered views exceed twelve; restore in a fresh Workshop");
            layout.views.push_back({saved.id, saved.kind, false, {}});
            view = &layout.views.back();
        }
        view->kind = saved.kind;
        for (const auto& id : saved.entries) view->entries.push_back({{}, id});
    }
    for (const auto& b : file.bindings)
        layout.bindings.push_back({{{}, b.entry}, b.target, b.scancode, b.modifiers, ++layout.serial, false});
    return layout;
}
inline void bind_toolbox_owner(InventoryViews& layout, const std::string& owner) {
    for (auto& v : layout.views) for (auto& ref : v.entries) ref.owner = owner;
    for (auto& b : layout.bindings) b.reference.owner = owner;
}

inline std::string toolbox_path(const std::string& path) {
    if (path.empty() || path.size() > 4096 || path.find('\0') != std::string::npos)
        throw std::invalid_argument("choose a toolbox file path");
    return std::filesystem::absolute(std::filesystem::path(path)).lexically_normal().generic_string();
}
inline InventoryToolbox read_toolbox(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::invalid_argument("cannot read toolbox: " + path);
    std::string bytes;
    std::array<char, 8192> chunk{};
    while (in) {
        in.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        const auto count = static_cast<std::size_t>(in.gcount());
        if (count > kMaxToolboxFileBytes - bytes.size())
            throw std::invalid_argument("toolbox file exceeds the 32 MiB limit");
        bytes.append(chunk.data(), count);
    }
    if (in.bad()) throw std::invalid_argument("toolbox read failed part way: " + path);
    const auto admitted = loom::admit(loom::parse(bytes), loom::schema_of<InventoryToolbox>());
    if (!admitted) throw std::invalid_argument("toolbox format refused: " + admitted.first_error().message());
    auto file = loom::from_value<InventoryToolbox>(admitted.value());
    validate_toolbox(file);
    return file;
}
inline void write_toolbox(const std::string& path, const InventoryToolbox& file) {
    validate_toolbox(file);
    const auto bytes = loom::serialize(loom::to_value(file));
    if (bytes.size() > kMaxToolboxFileBytes)
        throw std::invalid_argument("toolbox file exceeds the 32 MiB limit");
    const auto error = maker::write_file(path, bytes);
    if (!error.empty()) throw std::invalid_argument(error);
}
}
#endif
