// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INVENTORY_PANE_VOCABULARY_HPP
#define ZENGINE_INVENTORY_PANE_VOCABULARY_HPP
#include "inventory/vocabulary.hpp"
namespace zengine::inventory_pane {
inline constexpr const char* kRole = "zengine.inventory-pane";
struct SlotBinding {
    inventory::InventoryReference reference;
    std::string target;
    std::int64_t scancode = 0, modifiers = 0, serial = 0;
    bool enabled = false;
    ZEN_SHAPE(SlotBinding, 1, ZEN_FIELD(reference), ZEN_FIELD(target), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(serial), ZEN_FIELD(enabled));
};
struct SlotView {
    std::string id, kind;
    bool active = false;
    std::vector<inventory::InventoryReference> entries;
    ZEN_SHAPE(SlotView, 1, ZEN_FIELD(id), ZEN_FIELD(kind), ZEN_FIELD(active), ZEN_FIELD(entries));
};
struct InventoryViews {
    std::vector<SlotView> views;
    std::vector<SlotBinding> bindings;
    bool inventory_active = false;
    std::int64_t serial = 0;
    ZEN_SHAPE(InventoryViews, 1, ZEN_FIELD(views), ZEN_FIELD(bindings),
              ZEN_FIELD(inventory_active), ZEN_FIELD(serial));
};
struct InventoryViewsRequested { ZEN_SHAPE(InventoryViewsRequested, 1); };
// Explicit file operations. Relative paths use the Workshop process working directory.
struct InventoryToolboxSave {
    std::string path;
    ZEN_SHAPE(InventoryToolboxSave, 1, ZEN_FIELD(path));
};
struct InventoryToolboxRestore {
    std::string path;
    bool replace = false;
    ZEN_SHAPE(InventoryToolboxRestore, 1, ZEN_FIELD(path), ZEN_FIELD(replace));
};
struct InventoryToolboxFinished {
    std::string operation, path;
    std::int64_t entries = 0;
    ZEN_SHAPE(InventoryToolboxFinished, 1, ZEN_FIELD(operation), ZEN_FIELD(path), ZEN_FIELD(entries));
};
// create: kind in text, optional entry; move: destination view and optional before;
// context: view + enabled; bind: entry + target in text + key; enable: entry + enabled.
// Configuration and movement never execute. UI uses current actor permission for this door.
struct InventoryViewEdit {
    std::string operation, view, text;
    inventory::InventoryReference entry, before;
    std::int64_t scancode = 0, modifiers = 0;
    bool enabled = false;
    ZEN_SHAPE(InventoryViewEdit, 1, ZEN_FIELD(operation), ZEN_FIELD(view), ZEN_FIELD(text),
              ZEN_FIELD(entry), ZEN_FIELD(before), ZEN_FIELD(scancode), ZEN_FIELD(modifiers),
              ZEN_FIELD(enabled));
};
}
#endif
