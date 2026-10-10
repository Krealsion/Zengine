// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "flow/flow-pane/model.hpp"
#include "operator/primitives.hpp"
#include "restamp.hpp"
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
/// `add-node`'s arguments for an operator as the model's ports describe it now: its reference.
std::vector<std::string> ref_of(const pane::Model& m, const std::string& identity,
                                std::vector<std::string> where = {}) {
    for (const auto& p : m.palette)
        if (p.identity == identity) {
            std::vector<std::string> out{identity,
                std::to_string(static_cast<std::int64_t>(p.inputs->content_id())),
                std::to_string(static_cast<std::int64_t>(p.outputs->content_id()))};
            out.insert(out.end(), where.begin(), where.end());
            return out;
        }
    throw std::invalid_argument("no ports for " + identity);
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
    author.command("add-node", ref_of(author, "flowgraph.Rule"));
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

TEST_CASE("an operator reference is added at the end or before a node, renumbering what follows, "
          "and a stale one is refused, never re-bound") {
    auto author = model();
    author.command("trigger", {"0", "total"});
    author.command("add-node", ref_of(author, "flowgraph.Rule"));
    author.command("add-node", ref_of(author, "flowgraph.Rule"));
    author.command("bind", {"1", "0", "%0"});
    author.command("result", {"1"});
    const auto first_place = author.workspace.graph.place(0, 0).id;
    const auto second_place = author.workspace.graph.place(0, 1).id;

    // BEFORE NODE 0: the new node is %0, the two others move up one, and so do the binding that
    // named %0, the result and each place -- the graph means what it meant.
    author.command("add-node", ref_of(author, "flowgraph.Rule", {"0"}));
    // A command replaces the model whole, so the body is read again after each one.
    const auto body = [&]() -> const zengine::op::Composite& {
        return author.workspace.graph.project.definition.on.at(0).body;
    };
    REQUIRE(body().nodes.size() == 3);
    CHECK(body().nodes[2].arguments[0].from() == zengine::op::Binding::From::Node);
    CHECK(body().nodes[2].arguments[0].node_index() == 1);
    CHECK(body().result_node == 2);
    CHECK(author.workspace.graph.place(0, 1).id == first_place);
    CHECK(author.workspace.graph.place(0, 2).id == second_place);
    CHECK(author.node == std::optional<std::size_t>(0));

    // INTO A PORT: placed before the node whose port it fills, and wired there in the same edit.
    author.command("add-node-into", ref_of(author, "flowgraph.Rule", {"0", "0"}));
    REQUIRE(body().nodes.size() == 4);
    CHECK(body().nodes[1].arguments[0].from() == zengine::op::Binding::From::Node);
    CHECK(body().nodes[1].arguments[0].node_index() == 0);
    CHECK(body().nodes[3].arguments[0].node_index() == 2);
    CHECK(body().result_node == 3);
    (void)flow::read_workspace(flow::workspace_bytes(author.workspace));

    // A REFERENCE FOUND AT OTHER PORTS IS STALE, and a name nobody supplies is said so; the
    // draft is left as it was.
    auto stale = ref_of(author, "flowgraph.Rule");
    stale[1] = "12345";
    const auto revision = author.workspace.graph.project.definition.revision;
    try {
        author.command("add-node", stale);
        FAIL("a stale reference was added");
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()) == "'flowgraph.Rule' is not the operator this reference was "
                                       "found at: its ports changed since; find it again");
    }
    try {
        author.command("add-node", {"flowgraph.Gone", "1", "2"});
        FAIL("an unsupplied reference was added");
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()) == "nothing supplies 'flowgraph.Gone' here now");
    }
    CHECK(author.workspace.graph.project.definition.revision == revision);
    CHECK(body().nodes.size() == 4);
}

TEST_CASE("a fold is placed with its step bound to 1, its body chosen by reference with the count "
          "port the maker names, and its ports derive from that body") {
    pane::Model author;
    const auto add = zengine::op::make_operator<&zengine::op::add_int>(
        zengine::op::kAddInt, {"lhs", "rhs"}, "result");
    author.palette.push_back({add.identity(), add.inputs(), add.outputs()});
    author.command("state-field", {"total", "Int", "required"});
    author.command("message", {"tally.panel.Count"});
    for (const char* field : {"start", "limit", "step"})
        author.command("message-field", {"0", field, "Int", "required"});
    author.command("trigger", {"0", "total"});

    author.command("add-fold");
    const auto body = [&]() -> const zengine::op::Composite& {
        return author.workspace.graph.project.definition.on.at(0).body;
    };
    REQUIRE(body().nodes.size() == 1);
    REQUIRE(body().nodes[0].fold.has_value());
    CHECK(body().nodes[0].identity.empty());
    REQUIRE(body().nodes[0].arguments.size() == 3);
    CHECK(body().nodes[0].arguments[2].from() == zengine::op::Binding::From::Constant);
    CHECK(body().nodes[0].arguments[2].constant_cell().as_int() == 1);
    CHECK(author.workspace.graph.problems(author.palette).front() ==
          "tally.panel.Count / node 0: choose the fold's body");
    std::vector<std::string> counting;
    const auto unbodied = author.workspace.graph.node_ports(0, 0, author.palette);
    for (const auto& f : unbodied.inputs->fields()) counting.push_back(f.name);
    CHECK(counting == std::vector<std::string>{"start", "limit", "step"});

    // THE MAKER NAMES THE COUNT'S PORT; port names suggest, never decide.
    try {
        author.command("fold-body", {"0", "math.add",
            std::to_string(static_cast<std::int64_t>(add.inputs()->content_id())),
            std::to_string(static_cast<std::int64_t>(add.outputs()->content_id())), "count", "lhs"});
        FAIL("a fold took a body port that does not exist");
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()) == "'math.add' has no port 'count' for the count");
    }
    author.command("fold-body", {"0", "math.add",
        std::to_string(static_cast<std::int64_t>(add.inputs()->content_id())),
        std::to_string(static_cast<std::int64_t>(add.outputs()->content_id())), "rhs", "lhs"});
    CHECK(body().nodes[0].identity == "math.add");
    CHECK(body().nodes[0].fold->count == "rhs");
    CHECK(body().nodes[0].fold->accumulator == "lhs");
    const auto derived = author.workspace.graph.node_ports(0, 0, author.palette);
    std::vector<std::string> names;
    for (const auto& f : derived.inputs->fields()) names.push_back(f.name);
    CHECK(names == std::vector<std::string>{"start", "limit", "step", "initial"});
    CHECK(body().nodes[0].arguments[2].constant_cell().as_int() == 1); // the step kept its 1

    author.command("bind", {"0", "0", "$start"});
    author.command("bind", {"0", "1", "$limit"});
    author.command("bind", {"0", "2", "$step"});
    author.command("bind", {"0", "3", "0"});
    author.command("result", {"0"});
    CHECK(author.workspace.graph.problems(author.palette).empty());
    const auto run = author.command("run");
    CHECK(run.effect == pane::Effect::Run);
    const auto project = flow::read_project(flow::byte_string(run.payload));
    REQUIRE(project.definition.on.at(0).body.nodes.at(0).fold.has_value());
    CHECK(project.definition.on.at(0).body.nodes.at(0).fold->count == "rhs");
    const auto reopened = flow::read_workspace(flow::workspace_bytes(author.workspace));
    CHECK(reopened.graph.project.definition.on.at(0).body.nodes.at(0).fold->accumulator == "lhs");
}

TEST_CASE("emits are authored in the pane as the definition holds them: a message declared in the "
          "namespace, its fields written from same-named state fields or constants") {
    auto author = model();
    author.command("trigger", {"0", "total"});
    author.command("add-node", ref_of(author, "flowgraph.Rule"));
    author.command("bind", {"0", "0", "$input"});
    author.command("result", {"0"});
    author.command("emitted-message", {"Total"});
    author.command("emitted-field", {"0", "total", "Int", "required"});
    author.command("emitted-field", {"0", "note", "Int", "required"}); // no emit writes it yet
    const auto& emitted = author.workspace.graph.project.definition.emits;
    REQUIRE(emitted.size() == 1);
    CHECK(emitted[0]->name() == "my_flow.Total");

    // EACH FIELD FROM THE STATE FIELD OF ITS NAME unless the maker writes it.
    author.command("emit", {"0", "note=7"});
    const auto& on = author.workspace.graph.project.definition.on.at(0);
    REQUIRE(on.emits.size() == 1);
    REQUIRE(on.emits[0].fields.size() == 2);
    CHECK(on.emits[0].fields[0].field == "note");
    CHECK(on.emits[0].fields[0].constant->as_int() == 7);
    CHECK(on.emits[0].fields[1].field == "total");
    CHECK(on.emits[0].fields[1].source == std::optional<std::string>("total"));
    const auto run = author.command("run");
    const auto project = flow::read_project(flow::byte_string(run.payload));
    CHECK(project.definition.on.at(0).emits.size() == 1);
    CHECK(flow::read_workspace(flow::workspace_bytes(author.workspace)).graph.project.definition.on.at(0).emits.size() == 1);

    // REFUSED, the draft as it was: a field no state field writes, an emit on an empty trigger,
    // a message outside the namespace.
    const auto revision = author.workspace.graph.project.definition.revision;
    CHECK_THROWS_WITH_AS(author.command("emitted-field", {"0", "extra", "Int", "required"}),
        "the emit of my_flow.Total on my_flow.First would write no `extra`; remove that emit or "
        "declare state.extra", std::invalid_argument);
    author.command("emit-remove", {"0"});
    CHECK(author.workspace.graph.project.definition.on.at(0).emits.empty());
    author.command("trigger", {"1", "total"});
    CHECK_THROWS_WITH_AS(author.command("emit", {"0"}),
        "add the trigger's nodes before its emits: an empty trigger is kept without them",
        std::invalid_argument);
    CHECK_THROWS_AS(author.command("emitted-message", {"other.Said"}), std::invalid_argument);
    CHECK(author.workspace.graph.project.definition.revision == revision + 2);
}

TEST_CASE("a workspace and a project whose definition is version 1 still open, and save again at "
          "the current version") {
    auto author = model();
    author.command("trigger", {"0", "total"});
    author.command("add-node", ref_of(author, "flowgraph.Rule"));
    author.command("bind", {"0", "0", "$input"});
    author.command("result", {"0"});
    // The project bundle as an older build wrote it: its definition at version 1, inside.
    const auto as_v1 = [](const std::string& project) {
        const auto bundle = loom::admit(loom::parse(project), flow::project_schema()).value();
        const auto& d = bundle.get("definition")->as_bytes();
        const auto now = loom::admit(loom::parse(std::string(d.begin(), d.end())),
                                     zengine::maker::definition_schema()).value();
        auto old = zengine::testing::restamp(now, zengine::maker::definition_v1_schema());
        old.set("format_version", loom::Cell::integer(1));
        const auto older = loom::serialize(old);
        loom::Value out(flow::project_schema());
        out.set("definition", loom::Cell::bytes(loom::Bytes(older.begin(), older.end())));
        out.set("state", *bundle.get("state"));
        return loom::serialize(out);
    };
    const auto project_v1 = as_v1(flow::project_bytes(author.workspace.graph.project));
    const auto opened = flow::read_project(project_v1);
    CHECK(opened.definition.on.size() == 1);
    auto file = loom::from_value<flow::WorkspaceFile>(
        loom::admit(loom::parse(flow::workspace_bytes(author.workspace)),
                    loom::schema_of<flow::WorkspaceFile>()).value());
    file.project = flow::byte_vector(as_v1(flow::byte_string(file.project)));
    const auto workspace_v1 = loom::serialize(loom::to_value(file));
    const auto reopened = flow::read_workspace(workspace_v1);
    CHECK(reopened.graph.project.definition.on.at(0).body.nodes.at(0).identity == "flowgraph.Rule");
    const auto again = loom::admit(loom::parse(flow::workspace_bytes(reopened)),
                                   loom::schema_of<flow::WorkspaceFile>()).value();
    const auto inner = loom::admit(loom::parse(flow::byte_string(loom::from_value<flow::WorkspaceFile>(again).project)),
                                   flow::project_schema()).value();
    const auto& d = inner.get("definition")->as_bytes();
    CHECK(loom::parse(std::string(d.begin(), d.end())).claimed_version() ==
          zengine::maker::kDefinitionSchemaVersion);
}

TEST_CASE("a message shape keeps one model wherever it is authored: the pane and the workbench name it, add its fields and refuse a used name alike") {
    namespace shape = zengine::flow::shape;
    pane::Model m;
    m.command("message", {"Count"});
    for (const char* f : {"start", "limit", "step"}) m.command("message-field", {"0", f, "Int", "required"});
    zengine::op::Catalog catalog;
    flow::Draft draft(catalog, "my_flow");
    draft.command(flow::words("accept Count start:Int limit:Int step:Int"));
    auto made = shape::make(shape::qualified("my_flow", "Count"));
    for (const char* f : {"start", "limit", "step"})
        made = shape::with_field(*made, f, loom::type_of(loom::Kind::Int));
    const auto& in_pane = *m.workspace.graph.project.definition.accepts.at(0);
    CHECK(in_pane.name() == "my_flow.Count");
    CHECK(loom::same_identity(in_pane, *made));
    CHECK(loom::same_identity(*draft.definition().accepts.at(0), *made));
    CHECK_THROWS_WITH(m.command("message", {"Count"}), "a message named my_flow.Count is already declared");
    CHECK_THROWS_WITH(draft.command(flow::words("accept Count")), "a message named my_flow.Count is already declared");
    CHECK_THROWS_WITH(m.command("message-field", {"0", "step", "Int", "required"}),
                      "message field name is empty or already used");
    CHECK_THROWS_WITH(draft.command(flow::words("publish Said value:Int value:Int")),
                      "message field name is empty or already used");
    CHECK_THROWS_WITH(shape::with_field(*made, "step", loom::type_of(loom::Kind::Int)),
                      "message field name is empty or already used");
    CHECK(shape::type_named("List:Int", md::Library{}).kind == loom::Kind::List);
    CHECK_THROWS_WITH(shape::type_named("Message:none", md::Library{}),
                      "Message:<saved draft name> needs an existing library schema");
}
