// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_TESTS_FLOW_FIXTURE_HPP
#define ZENGINE_TESTS_FLOW_FIXTURE_HPP

#include "flow/example.hpp"
#include "operator/primitives.hpp"

namespace flowfix {
namespace op = zengine::op;
namespace maker = zengine::maker;

inline std::shared_ptr<const loom::Schema> nested() {
    return loom::SchemaBuilder("flowtest.Nested", 1).field("text", loom::Kind::Text).build();
}

inline std::vector<std::pair<std::string, loom::TypeRef>> kinds() {
    return {{"integer", loom::type_of(loom::Kind::Int)},
            {"real", loom::type_of(loom::Kind::Float)},
            {"text", loom::type_of(loom::Kind::Text)},
            {"flag", loom::type_of(loom::Kind::Bool)},
            {"bytes", loom::type_of(loom::Kind::Bytes)},
            {"nested", loom::type_message(nested())},
            {"list", loom::type_list(loom::type_message(nested()))}};
}

inline std::string nul_identity() { return std::string("flowtest.echo\0integer", 21); }

inline op::OperatorDef echo(const std::string& name, const loom::TypeRef& type) {
    return op::OperatorDef(name, loom::make_schema(name + ".in", 1, {{"argument", type, true}}),
        loom::make_schema(name + ".out", 1, {{"result", type, true}}),
        [](const loom::Value& input) { return *input.at(0); });
}

inline void operators(op::Catalog& catalog) {
    op::publish_primitives(catalog);
    std::vector<op::OperatorDef> defs;
    for (const auto& [name, type] : kinds()) defs.push_back(echo("flowtest.echo_" + name, type));
    defs.push_back(echo(nul_identity(), loom::type_of(loom::Kind::Int)));
    defs.push_back(echo("flowtest.after", loom::type_of(loom::Kind::Int)));
    const auto mounted = catalog.mount("flowtest.operators", std::move(defs));
    if (!mounted) throw std::runtime_error(mounted.reason);
}

inline maker::Definition boundaries(const op::Catalog& catalog) {
    maker::Definition definition;
    definition.name = "flowtest";
    std::vector<loom::Field> state;
    for (const auto& [name, type] : kinds()) state.push_back({name, type, true});
    state.push_back({"absent", loom::type_of(loom::Kind::Int), false});
    definition.state = loom::make_schema("flowtest.State", 1, std::move(state));
    const auto good = loom::SchemaBuilder("flowtest.Written", 1).field("value", loom::Kind::Int).build();
    const auto bad = loom::SchemaBuilder("flowtest.Missing", 1).field("value", loom::Kind::Int).build();
    definition.emits = {good, bad};
    auto add = [&](std::string message, loom::TypeRef type, const std::string& output,
                   const std::string& operator_name, bool optional = false) -> maker::On& {
        auto shape = loom::make_schema(std::move(message), 1, {{"incoming", std::move(type), !optional}});
        definition.accepts.push_back(shape);
        maker::On trigger;
        trigger.message = shape; trigger.output = output;
        const auto* target = catalog.find(operator_name);
        trigger.body.nodes.push_back({operator_name, {op::Binding::input("incoming")},
                                      target->inputs()->content_id(), target->outputs()->content_id()});
        definition.on.push_back(std::move(trigger));
        return definition.on.back();
    };
    for (const auto& [name, type] : kinds()) add("flowtest.Set_" + name, type, name, "flowtest.echo_" + name);
    const auto int_type = loom::type_of(loom::Kind::Int);
    add("flowtest.Optional", int_type, "integer", "flowtest.echo_integer", true);
    add("flowtest.Nul", int_type, "integer", nul_identity());
    auto& extra = add("flowtest.Extra", int_type, "integer", "flowtest.echo_integer", true);
    extra.body.nodes[0].arguments = {op::Binding::constant(loom::Cell::integer(71)), op::Binding::input("incoming")};
    auto& late = add("flowtest.Late", int_type, "integer", "flowtest.echo_integer");
    const auto* after = catalog.find("flowtest.after");
    late.body.nodes.push_back({"flowtest.after", {op::Binding::node(0)},
                              after->inputs()->content_id(), after->outputs()->content_id()});
    late.body.result_node = 0;
    add("flowtest.Wrong", loom::type_of(loom::Kind::Bool), "integer", "flowtest.echo_flag");
    auto& emit = add("flowtest.Emit", int_type, "integer", "flowtest.echo_integer");
    emit.emits.push_back({bad, {{"value", std::string("absent"), std::nullopt}}});
    emit.emits.push_back({good, {{"value", std::string("integer"), std::nullopt}}});
    auto admitted = maker::read_definition(maker::definition_bytes(definition));
    if (!admitted) throw std::runtime_error(admitted.reason);
    return std::move(admitted.definition);
}
} // namespace flowfix
#endif
