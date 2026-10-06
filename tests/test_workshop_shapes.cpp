// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// WHAT THIS TREE'S SHAPES PUBLISH. Loom names a shape by its bare name and version, folds a nested
// shape's content id into its parent's, and refuses a second shape under a name and version it
// already holds -- so a shape that encloses a changed shape is a new version too, or a component
// built before the change and one built after cannot load side by side.

#include "doctest.h"

#include "workshop/desktop_seam_vocabulary.hpp"
#include "view-builder/vocabulary.hpp"
#include "builder/weave.hpp"

#include <zen/registry.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace ws = zengine::workshop;
namespace vb = zengine::view_builder;

// THE SHAPES AS ZENGINE 5901b39 PUBLISHED THEM, copied field for field. Their content ids are
// pinned in the case below, read off that tree itself (VM-FIX-20), so a copy that drifted from
// what was published fails there first.
namespace published {

struct InventoryPane {
    std::string office;
    std::string pane;
    std::string name;
    std::string summary;
    bool open = false;
    bool available = false;
    bool waiting = false;
    bool pending = false;
    ZEN_SHAPE(InventoryPane, 1, ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(name),
              ZEN_FIELD(summary), ZEN_FIELD(open), ZEN_FIELD(available), ZEN_FIELD(waiting),
              ZEN_FIELD(pending));
};

struct PaneInventory {
    std::vector<InventoryPane> panes;
    ZEN_SHAPE(PaneInventory, 1, ZEN_FIELD(panes));
};

/// The View Builder's reload state as it was published, under the name it then had.
struct BuilderState {
    loom::Bytes description;
    std::string path, file;
    bool dirty = false, running = false;
    ZEN_SHAPE(BuilderState, 2, ZEN_FIELD(description), ZEN_FIELD(path), ZEN_FIELD(file),
              ZEN_FIELD(dirty), ZEN_FIELD(running));
};

} // namespace published

} // namespace

TEST_CASE("a shape that encloses a changed shape is a new version, so the shapes published before claim beside the current ones in one registry") {
    // ⚔ MUTATION: `PaneInventory` kept at version 1 while the `InventoryPane` it carries moved to
    // version 2 -- two shapes under one name and version, and the registry refuses the second.
    //
    // THE PUBLISHED IDS, compiled from Zengine 5901b39 in a worktree of it (the shapes' own
    // headers, not these copies), so the copies above are the shapes that were published.
    CHECK(loom::schema_of<published::InventoryPane>()->content_id() == 0x2d2acc35864aed07ull);
    CHECK(loom::schema_of<published::PaneInventory>()->content_id() == 0x322ddeacaa13391eull);
    CHECK(loom::schema_of<published::BuilderState>()->content_id() == 0xcc8502d593469770ull);
    // ONE REGISTRY, as one Loom holds a presenter built before and a Workshop built after: every
    // shape a change touched, and every shape that encloses one, as published and as it is now.
    const std::vector<std::shared_ptr<const loom::Schema>> shapes = {
        loom::schema_of<published::InventoryPane>(), loom::schema_of<published::PaneInventory>(),
        loom::schema_of<published::BuilderState>(),  loom::schema_of<ws::InventoryPane>(),
        loom::schema_of<ws::PaneInventory>(),        loom::schema_of<vb::BuilderState>(),
        loom::schema_of<zengine::builder::BuilderState>(),
    };
    loom::Registry registry;
    std::string conflict;
    try {
        const loom::SchemaClaimScope claimed = registry.claim(shapes);
        (void)claimed;
    } catch (const loom::SchemaConflict& refused) {
        conflict = refused.what();
    }
    CHECK_MESSAGE(conflict.empty(), conflict);
}
