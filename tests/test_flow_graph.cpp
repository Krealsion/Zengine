// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "flow-pane/model.hpp"
#include <chrono>
#include <filesystem>

namespace {
namespace flow = zengine::flow;
namespace pane = zengine::flow_pane;
namespace md = zengine::message_draft;
flow::Ports ports() {
    return {"flowgraph.Rule", loom::SchemaBuilder("flowgraph.In", 1).field("a", loom::Kind::Int).build(),
        loom::SchemaBuilder("flowgraph.Out", 1).field("value", loom::Kind::Int).build()};
}
pane::Model model() {
    pane::Model out;
    out.palette.push_back(ports());
    out.command("state-field", {"total", "Int", "required"});
    out.command("message", {"First"});
    out.command("message-field", {"0", "input", "Int", "required"});
    out.command("message", {"Second"});
    out.command("message-field", {"1", "other", "Bool", "required"});
    return out;
}
struct TempWorkspace {
    std::filesystem::path directory;
    TempWorkspace() {
        directory = std::filesystem::temp_directory_path() / ("zengine-flow-forms-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(directory)) throw std::runtime_error("temporary directory collision");
    }
    ~TempWorkspace() { std::error_code error; std::filesystem::remove_all(directory, error); }
    std::string path() const { return (directory / "workspace.zen").string(); }
};
}

TEST_CASE("graph workspace preserves empty triggers and unwired ports without admitting them for execution") {
    auto author = model();
    author.command("trigger", {"0", "total"});
    author.command("trigger", {"1", "total"});
    author.command("add-node", {"flowgraph.Rule"});
    const auto bytes = flow::workspace_bytes(author.workspace);
    auto reopened = flow::read_workspace(bytes);
    REQUIRE(reopened.graph.project.definition.on.size() == 2);
    CHECK(reopened.graph.project.definition.on[0].body.nodes.empty());
    REQUIRE(reopened.graph.project.definition.on[1].body.nodes.size() == 1);
    const auto& argument = reopened.graph.project.definition.on[1].body.nodes[0].arguments.at(0);
    CHECK(argument.from() == zengine::op::Binding::From::Input);
    CHECK(argument.input_name().empty());
    REQUIRE(reopened.graph.places.size() == 1);
    CHECK(reopened.graph.places.front().trigger == 1);
    CHECK_FALSE(reopened.graph.problems(author.palette).empty());
    CHECK_THROWS_AS(flow::project_bytes(reopened.graph.project), std::invalid_argument);
    CHECK_THROWS_AS(author.command("run"), std::invalid_argument);
}

TEST_CASE("editing message forms dirties and preserves each schema across switches and save reopen") {
    auto author = model();
    author.command("message-open", {"0"});
    author.dirty = false;
    author.command("value", {"0", "37"});
    CHECK(author.dirty);
    author.command("message-open", {"1"});
    author.command("value", {"0", "false"});
    author.command("message-open", {"0"});
    REQUIRE(author.form);
    CHECK(author.form->get({std::string("input")})->as_int() == 37);
    TempWorkspace file;
    author.command("save", {file.path()});
    CHECK_FALSE(author.dirty);
    pane::Model fresh;
    fresh.command("open", {file.path(), ""});
    REQUIRE(fresh.form);
    CHECK(fresh.form->get({std::string("input")})->as_int() == 37);
    fresh.command("message-open", {"1"});
    REQUIRE(fresh.form->get({std::string("other")}));
    CHECK_FALSE(fresh.form->get({std::string("other")})->as_bool());
}

TEST_CASE("unfinished state form survives independently of executable initial state") {
    auto author = model();
    author.command("state-open");
    author.dirty = false;
    author.command("unset", {"0"});
    CHECK(author.dirty);
    REQUIRE(author.form);
    CHECK_FALSE(author.form->ready());
    const auto before = loom::serialize(author.workspace.graph.project.state);
    CHECK_THROWS_AS(author.command("keep-state"), std::invalid_argument);
    CHECK(loom::serialize(author.workspace.graph.project.state) == before);
    pane::Model restored;
    restored.workspace = flow::read_workspace(flow::workspace_bytes(author.workspace));
    restored.restore_form();
    REQUIRE(restored.form);
    CHECK(restored.form_state);
    CHECK(restored.form->get({std::string("total")}) == nullptr);
    CHECK(loom::admit(restored.workspace.graph.project.state, *restored.workspace.graph.project.definition.state));
    const auto epoch = restored.state_epoch;
    restored.command("value", {"0", "23"});
    CHECK_FALSE(restored.state_edited);
    restored.command("keep-state");
    CHECK(restored.state_edited);
    CHECK(restored.state_epoch == epoch + 1);
    CHECK(restored.workspace.graph.project.state.get("total")->as_int() == 23);
}

TEST_CASE("editing a schema retains its old form explicitly instead of converting its authored data") {
    auto author = model();
    author.command("message-open", {"0"});
    author.command("value", {"0", "52"});
    const auto old = author.form->schema();
    const auto old_key = author.form_key;
    author.command("message-field", {"0", "new_field", "Text", "required"});
    CHECK(author.notice.find("previous draft remains") != std::string::npos);
    REQUIRE(author.form);
    CHECK_FALSE(loom::same_identity(*old, *author.form->schema()));
    CHECK(author.form->get({std::string("input")}) == nullptr);
    const auto saved = flow::read_workspace(flow::workspace_bytes(author.workspace));
    const auto prior = std::find_if(saved.forms.begin(), saved.forms.end(), [&](const auto& entry) { return entry.key == old_key; });
    REQUIRE(prior != saved.forms.end());
    CHECK(prior->draft.get({std::string("input")})->as_int() == 52);
    const auto at = static_cast<std::size_t>(prior - saved.forms.begin());
    author.command("draft-open", {std::to_string(at)});
    CHECK(loom::same_identity(*author.form->schema(), *old));
    CHECK(author.form->get({std::string("input")})->as_int() == 52);
}

TEST_CASE("reusable preset editing retains the separate message form") {
    auto author = model();
    author.command("message-open", {"0"});
    author.command("value", {"0", "11"});
    author.command("preset", {"Sample"});
    author.command("value", {"0", "17"});
    author.command("preset-open", {"Sample"});
    CHECK(author.form->get({std::string("input")})->as_int() == 11);
    author.command("value", {"0", "29"});
    author.command("message-open", {"0"});
    CHECK(author.form->get({std::string("input")})->as_int() == 17);
    author.command("preset-open", {"Sample"});
    CHECK(author.form->get({std::string("input")})->as_int() == 29);
    CHECK(author.workspace.library.find("Sample")->get({std::string("input")})->as_int() == 11);
}

TEST_CASE("model command refusal changes neither draft nor selection or dirty state") {
    auto author = model();
    author.command("message-open", {"0"});
    author.command("value", {"0", "7"});
    author.dirty = false;
    const auto before = flow::workspace_bytes(author.workspace);
    const auto selection = author.message;
    CHECK_THROWS_AS(author.command("value", {"0", "not a number"}), std::invalid_argument);
    CHECK_THROWS_AS(author.command("message-field", {"100", "bad", "Int", "required"}), std::out_of_range);
    CHECK_THROWS_AS(author.command("state-field", {"new", "Int", "requried"}), std::invalid_argument);
    CHECK(flow::workspace_bytes(author.workspace) == before);
    CHECK(author.message == selection);
    CHECK_FALSE(author.dirty);
    CHECK(author.form->get({std::string("input")})->as_int() == 7);
}

TEST_CASE("nested container edits are retained and explicit list removal is available") {
    pane::Model author;
    author.command("message", {"Nested"});
    author.command("message-field", {"0", "values", "List:Int", "required"});
    author.dirty = false;
    author.command("container", {"0"});
    CHECK(author.dirty);
    author.command("container", {"0"});
    author.command("value", {"1", "13"});
    pane::Model restored;
    restored.workspace = flow::read_workspace(flow::workspace_bytes(author.workspace));
    restored.restore_form();
    REQUIRE(restored.form);
    CHECK(restored.form->get({std::string("values"), std::size_t(0)})->as_int() == 13);
    restored.command("list-erase", {"0", "0"});
    CHECK(restored.form->get({std::string("values")})->as_list().empty());

    const auto leaf = loom::SchemaBuilder("test.Leaf", 1).field("count", loom::Kind::Int).build();
    restored.workspace.library.put("Leaf", md::Draft(leaf));
    restored.command("message-field", {"0", "leaves", "List:Message:Leaf", "required"});
    restored.command("container", {"1"});
    restored.command("container", {"1"});
    CHECK(restored.form->get({std::string("leaves"), std::size_t(0), std::string("count")}) == nullptr);
    restored.command("value", {"3", "31"});
    restored.command("container", {"2"});
    CHECK(restored.form->get({std::string("leaves"), std::size_t(0), std::string("count")})->as_int() == 31);
}

TEST_CASE("workspace rejects corrupt signed geometry and dangling active forms") {
    auto author = model();
    const auto admitted = loom::admit(loom::parse(flow::workspace_bytes(author.workspace)), loom::schema_of<flow::WorkspaceFile>());
    REQUIRE(admitted);
    auto file = loom::from_value<flow::WorkspaceFile>(admitted.value());
    file.pan_x = std::numeric_limits<std::int64_t>::min();
    CHECK_THROWS_AS(flow::read_workspace(loom::serialize(loom::to_value(file))), std::invalid_argument);
    file.pan_x = 0;
    file.active_form = "missing";
    CHECK_THROWS_AS(flow::read_workspace(loom::serialize(loom::to_value(file))), std::invalid_argument);
    file.active_form = author.workspace.active_form;
    REQUIRE_FALSE(file.forms.empty());
    file.forms.push_back(file.forms.front());
    CHECK_THROWS_AS(flow::read_workspace(loom::serialize(loom::to_value(file))), std::invalid_argument);
}

TEST_CASE("form restoration selects only an exactly matching current message") {
    auto author = model();
    author.command("message-open", {"1"});
    author.command("value", {"0", "true"});
    pane::Model restored;
    restored.workspace = flow::read_workspace(flow::workspace_bytes(author.workspace));
    CHECK(restored.message == 0);
    restored.restore_form();
    CHECK(restored.message == 1);
    const auto old_key = restored.form_key;
    restored.command("message-field", {"1", "more", "Text", "optional"});
    restored.command("message-open", {"0"});
    restored.workspace.active_form = old_key;
    restored.restore_form();
    CHECK(restored.message == 0); // historical shape is not silently retargeted by name
    REQUIRE(restored.form);
    CHECK(restored.form->schema()->find("more") == nullptr);
    CHECK(restored.form->get({std::string("other")})->as_bool());
}

TEST_CASE("form type spelling rejects excessive list nesting without recursive parsing") {
    pane::Model author;
    std::string prefixes;
    for (int i = 0; i < 64; ++i) prefixes += "List:";
    CHECK_NOTHROW(author.type(prefixes + "Int"));
    CHECK_THROWS_AS(author.type(prefixes + "List:Int"), std::invalid_argument);
    for (int i = 0; i < 1000; ++i) prefixes += "List:";
    const auto before = flow::workspace_bytes(author.workspace);
    CHECK_THROWS_AS(author.command("state-field", {"deep", prefixes + "Int", "required"}), std::invalid_argument);
    CHECK(flow::workspace_bytes(author.workspace) == before);
}

TEST_CASE("aggregate workspace refusal preserves a reloadable authoring state") {
    pane::Model author;
    author.command("message", {"TextInput"});
    author.command("message-field", {"0", "text", "Text", "required"});
    author.command("value", {"0", std::string(32768, 'x')});
    bool refused = false;
    for (int i = 0; i < 100; ++i) {
        const auto before = flow::workspace_bytes(author.workspace);
        const auto entries = author.workspace.library.entries().size();
        try { author.command("preset", {"example-" + std::to_string(i)}); }
        catch (const std::invalid_argument&) {
            refused = true;
            CHECK(flow::workspace_bytes(author.workspace) == before);
            CHECK(author.workspace.library.entries().size() == entries);
            pane::Model restored;
            restored.workspace = flow::read_workspace(before);
            restored.restore_form();
            REQUIRE(restored.form);
            CHECK(restored.form->get({std::string("text")})->as_text() == std::string(32768, 'x'));
            break;
        }
    }
    CHECK(refused);
}
