// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_PANE_SLOTS_HPP
#define ZENGINE_INVENTORY_PANE_SLOTS_HPP
#include "vocabulary.hpp"
#include "workshop/pane_shortcuts.hpp"
#include <algorithm>
#include <stdexcept>
namespace zengine::inventory_pane {
inline std::string key(const inventory::InventoryReference& r) { return r.owner + ":" + r.entry; }
inline bool same(const inventory::InventoryReference& a, const inventory::InventoryReference& b) {
    return a.owner == b.owner && a.entry == b.entry;
}
inline SlotView* find(InventoryViews& s, const std::string& id) {
    for (auto& v : s.views) if (v.id == id) return &v;
    return nullptr;
}
inline const SlotBinding* binding(const InventoryViews& s, const inventory::InventoryReference& r) {
    for (const auto& b : s.bindings) if (same(b.reference, r)) return &b;
    return nullptr;
}
inline std::string placed(const InventoryViews& s, const inventory::InventoryReference& r) {
    for (const auto& v : s.views) for (const auto& e : v.entries) if (same(e, r)) return v.id;
    return "inventory";
}
inline void move(InventoryViews& s, const inventory::InventoryReference& r,
                 const std::string& into, const inventory::InventoryReference& before = {}) {
    auto* destination = find(s, into);
    if (into != "inventory" && !destination) throw std::invalid_argument("That inventory view is unavailable");
    if (same(r, before)) return;
    if (destination && !before.entry.empty() && std::none_of(destination->entries.begin(),
        destination->entries.end(), [&](const auto& e) { return same(e, before); }))
        throw std::invalid_argument("The destination entry moved; try its current position");
    for (auto& v : s.views) std::erase_if(v.entries, [&](const auto& e) { return same(e, r); });
    if (!destination) return;
    if (destination->kind == "single") destination->entries.clear(); // displaced value remains owned
    auto at = std::find_if(destination->entries.begin(), destination->entries.end(),
                           [&](const auto& e) { return same(e, before); });
    destination->entries.insert(at, r);
}
inline InventoryViews edited(InventoryViews s, const InventoryViewEdit& op) {
    if (op.operation == "create") {
        if (s.views.size() >= 12) throw std::invalid_argument("At most twelve portable inventory views");
        if (op.text != "single" && op.text != "row" && op.text != "column")
            throw std::invalid_argument("Choose single, row or column");
        const auto id = "inventory." + std::to_string(++s.serial);
        s.views.push_back({id, op.text, false, {}});
        if (!op.entry.entry.empty()) move(s, op.entry, id);
    } else if (op.operation == "move") move(s, op.entry, op.view, op.before);
    else if (op.operation == "context") {
        if (op.view == "inventory") s.inventory_active = op.enabled;
        else if (auto* v = find(s, op.view)) v->active = op.enabled;
        else throw std::invalid_argument("That inventory view is unavailable");
    } else if (op.operation == "bind") {
        if (op.text.empty() || op.text.size() > 128 || op.scancode <= 0)
            throw std::invalid_argument("A binding needs an explicit target office and key");
        auto it = std::find_if(s.bindings.begin(), s.bindings.end(), [&](const auto& b) { return same(b.reference, op.entry); });
        if (it == s.bindings.end()) {
            if (s.bindings.size() >= 16) throw std::invalid_argument("At most sixteen configured command bindings");
            s.bindings.push_back({op.entry, op.text, op.scancode, op.modifiers, ++s.serial, false});
        } else { it->target = op.text; it->scancode = op.scancode; it->modifiers = op.modifiers; }
    } else if (op.operation == "enable") {
        auto it = std::find_if(s.bindings.begin(), s.bindings.end(), [&](const auto& b) { return same(b.reference, op.entry); });
        if (it == s.bindings.end()) throw std::invalid_argument("Configure this item's key and target first");
        it->enabled = op.enabled;
    } else throw std::invalid_argument("Unknown inventory view operation");
    return s;
}
inline std::string action(const SlotBinding& b) { return "slot." + std::to_string(b.serial); }
inline std::vector<workshop::PaneShortcut> shortcuts(const InventoryViews& s) {
    std::vector<workshop::PaneShortcut> out;
    for (const auto& b : s.bindings) {
        if (!b.enabled) continue;
        const auto placement = placed(s, b.reference);
        bool active = s.inventory_active;
        for (const auto& v : s.views) if (v.id == placement) active = v.active;
        if (active) out.push_back({action(b), placement + " / " + action(b), "inventory", action(b), b.scancode, b.modifiers});
    }
    return out;
}
inline void duplicate_binding(InventoryViews& s, const inventory::InventoryReference& source,
                              const inventory::InventoryReference& copy) {
    if (const auto* b = binding(s, source)) {
        if (s.bindings.size() >= 16) throw std::invalid_argument("No room to retain the duplicate's binding");
        auto value = *b; value.reference = copy; value.enabled = false; value.serial = ++s.serial;
        s.bindings.push_back(std::move(value));
    }
}
inline std::string duplicate_name(const std::string& base, const std::vector<inventory::InventorySummary>& entries) {
    auto stem = base.empty() ? std::string("Item") : base;
    // Existing trailing numbers increment rather than accumulating suffixes.
    std::size_t cut = stem.size(); while (cut && stem[cut-1] >= '0' && stem[cut-1] <= '9') --cut;
    std::uint64_t number = 1;
    if (cut < stem.size() && cut && stem[cut-1] == ' ') {
        try { number = std::min<std::uint64_t>(std::stoull(stem.substr(cut)), 1000000); stem.resize(cut-1); } catch (...) {}
    }
    for (;;) {
        const auto suffix = " " + std::to_string(++number);
        const auto name = stem.substr(0, 80 - suffix.size()) + suffix;
        if (std::none_of(entries.begin(), entries.end(), [&](const auto& e) { return e.label == name; })) return name;
    }
}
}
#endif
