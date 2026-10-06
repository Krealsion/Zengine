// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// WHAT THIS TREE'S SHAPES PUBLISH. Loom names a shape by its bare name and version, folds a nested
// shape's content id into its parent's, and refuses a second shape under a name and version it
// already holds -- so a shape that encloses a changed shape is a new version too, or a component
// built before the change and one built after cannot load side by side.

#include "doctest.h"
#include "workshop_support.hpp"

#include "workshop/desktop_seam_vocabulary.hpp"
#include "view-builder/vocabulary.hpp"
#include "builder/weave.hpp"

#include <zen/registry.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
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

/// The shape names a source declares: each `ZEN_SHAPE(<name>, ...)`, in order.
std::vector<std::string> shape_names_in(const std::string& text) {
    static const std::regex shape(R"(\bZEN_SHAPE\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,)");
    std::vector<std::string> out;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), shape);
         it != std::sregex_iterator(); ++it) {
        out.push_back((*it)[1].str());
    }
    return out;
}

/// EVERY SHAPE NAME THE TREE'S COMPONENTS DECLARE, with the top-level directories declaring it.
/// The tests keep copies of published shapes on purpose, and build trees and vendored code are not
/// this tree's own, so none of them is read.
std::map<std::string, std::set<std::string>> declared_shape_names(const std::filesystem::path& root,
                                                                  std::size_t& declarations) {
    namespace fs = std::filesystem;
    std::map<std::string, std::set<std::string>> out;
    for (auto it = fs::recursive_directory_iterator(root); it != fs::recursive_directory_iterator();
         ++it) {
        const std::string name = it->path().filename().string();
        if (it->is_directory()) {
            const bool top = it.depth() == 0;
            if (name == ".git" || name == "third_party" ||
                (top && (name == "tests" || name == "docs" || name == "reference" ||
                         name == "build" || name.rfind("build-", 0) == 0 ||
                         name.rfind("cmake-build", 0) == 0 || name == "_install"))) {
                it.disable_recursion_pending();
            }
            continue;
        }
        const std::string ext = it->path().extension().string();
        if (ext != ".hpp" && ext != ".cpp" && ext != ".h" && ext != ".ipp" && ext != ".inl") {
            continue;
        }
        std::ifstream in(it->path(), std::ios::binary);
        const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        const std::string component = fs::relative(it->path(), root).begin()->string();
        for (const std::string& shape : shape_names_in(text)) {
            out[shape].insert(component);
            ++declarations;
        }
    }
    return out;
}

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
        loom::schema_of<ws::PaneInventory>(),        loom::schema_of<vb::ViewBuilderState>(),
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

TEST_CASE("no two components of this tree declare a shape of one name") {
    // ⚔ MUTATION: a component naming its state as another's is named -- Loom claims a library's
    // state shape and vocabulary at load, so the next version either of the two took would meet
    // the other's under one name and version, and one of the two libraries would be refused.
    //
    // THE READER, MADE TO SAY YES AND NO before it is believed.
    CHECK(shape_names_in("ZEN_SHAPE(Alpha, 1, ZEN_FIELD(x));\nZEN_SHAPE( Beta ,2);") ==
          std::vector<std::string>{"Alpha", "Beta"});
    CHECK(shape_names_in("NOT_ZEN_SHAPE(Gamma, 1); ZEN_SHAPES(Delta, 1); ZEN_SHAPE(1x, 1);").empty());
    std::size_t declarations = 0;
    const auto names = declared_shape_names(ZENGINE_SOURCE_DIR, declarations);
    MESSAGE("read " << declarations << " shape declarations, " << names.size() << " names, under "
                    << ZENGINE_SOURCE_DIR);
    // A scan that found nothing would pass for want of anything to find.
    REQUIRE(declarations > 100);
    for (const auto& [shape, components] : names) {
        std::string where;
        for (const std::string& component : components) {
            where += (where.empty() ? "" : ", ") + component;
        }
        const std::string said = shape + " is declared by " + where;
        CHECK_MESSAGE(components.size() == 1, said);
    }
}

TEST_CASE("the test rig's booter loads a library after a plan has run, its state no shape of Workshop's") {
    // ⚔ MUTATION: the rig's booter state named as the plan booter's -- `BootState` v1 of another
    // shape -- and the load after a realized plan is refused by Loom's registry.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    (void)r.run_plan(load::LoadPlan{});
    CHECK_NOTHROW(r.load_presenter());
    CHECK(r.load_refusals.empty());
}
