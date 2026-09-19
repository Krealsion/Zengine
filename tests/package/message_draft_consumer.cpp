// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "message-draft/library.hpp"
#include "flow/workspace.hpp"
#include "flow-host/vocabulary.hpp"
#include "flow-pane/vocabulary.hpp"
#include "workshop/pane_canvas_vocabulary.hpp"
#include <iostream>

namespace md = zengine::message_draft;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        zengine::flow::Workspace empty;
        auto workspace = zengine::flow::read_workspace(zengine::flow::workspace_bytes(empty));
        require(workspace.graph.project.definition.name == "my_flow", "installed workspace round trip failed");
        require(loom::schema_of<zengine::flow_host::FlowRun>()->find("project") != nullptr, "runtime protocol missing");
        require(loom::schema_of<zengine::flow_pane::FlowEdit>()->find("action") != nullptr, "authoring protocol missing");
        require(loom::schema_of<zengine::workshop::PaneCanvasContent>()->find("picture") != nullptr, "canvas protocol missing");
        const auto item = loom::SchemaBuilder("stranger.Item", 2)
            .field("caption", loom::Kind::Text).field("enabled", loom::Kind::Bool).build();
        const auto shape = loom::SchemaBuilder("stranger.Example", 1)
            .field("count", loom::Kind::Int)
            .list("items", loom::type_message(item)).build();
        md::Draft authored(shape);
        authored.create_list({std::string("items")});
        authored.append({std::string("items")}, loom::Cell::message(loom::Value(item)));
        authored.set_text({std::string("items"), std::size_t(0), std::string("enabled")}, "false");

        md::Library library;
        library.put("Unfinished example", authored);
        auto reopened = md::read_library(md::library_bytes(library));
        auto* draft = reopened.find("Unfinished example");
        require(draft != nullptr, "installed draft library lost its named entry");
        require(loom::same_identity(*draft->schema(), *shape), "installed codec lost original schema identity");
        const auto* items = draft->get({std::string("items")});
        require(items != nullptr && items->as_list().size() == 1, "nested list did not survive");
        const auto& nested = *items->as_list()[0].as_message();
        require(loom::same_identity(nested.schema(), *item), "schema closure lost nested identity");
        require(!nested.has("caption"), "unfinished required field was defaulted");
        require(nested.get("enabled") && !nested.get("enabled")->as_bool(), "false became absence");
        require(!draft->admit(), "an incomplete draft passed complete runtime admission");

        draft->set_text({std::string("count")}, "41");
        draft->set_text({std::string("items"), std::size_t(0), std::string("caption")}, "007");
        auto complete = draft->admit(*shape);
        require(static_cast<bool>(complete), "completed draft failed original-schema admission");
        require(complete.value().get("count")->as_int() == 41, "scalar compose changed count");
        require(complete.value().get("items")->as_list()[0].as_message()->get("caption")->as_text() == "007",
                "schema-directed Text became a number");

        loom::Registry current;
        const auto conflict = loom::SchemaBuilder("stranger.Item", 2).field("other", loom::Kind::Int).build();
        current.register_schema(conflict);
        bool refused = false;
        try { (void)md::read_library(md::library_bytes(reopened), &current); }
        catch (const loom::SchemaConflict&) { refused = true; }
        require(refused, "reopening silently accepted a conflicting nested schema");
        require(current.size() == 1 && loom::same_identity(*current.lookup("stranger.Item", 2), *conflict),
                "compatibility inspection modified the current registry");
        std::cout << "installed message drafts preserve incomplete values, schema closure and complete admission\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
