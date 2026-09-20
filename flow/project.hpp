// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PROJECT_HPP
#define ZENGINE_FLOW_PROJECT_HPP

#include "maker/definition.hpp"
#include "maker/files.hpp"
#include "maker/write.hpp"

#include <string>

namespace zengine::flow {

// A save bundles the two native maker artifacts in one atomic publication. The
// nested bytes remain owned by their original codecs; this is not another definition.
inline std::shared_ptr<const loom::Schema> project_schema() {
    static const auto schema = loom::SchemaBuilder("zengine.flow.Project", 1)
        .field("definition", loom::Kind::Bytes).field("state", loom::Kind::Bytes).build();
    return schema;
}

struct Project {
    maker::Definition definition;
    loom::Value state;
};

inline Project make_project(maker::Definition definition, loom::Value state) {
    auto accepted = maker::read_definition(maker::definition_bytes(definition));
    if (!accepted) throw std::invalid_argument(accepted.reason);
    auto checked = loom::admit(std::move(state), *accepted.definition.state);
    if (!checked) throw std::invalid_argument(checked.first_error().message());
    return {std::move(accepted.definition), std::move(checked).value()};
}

inline std::string project_bytes(const Project& project) {
    const auto checked = make_project(project.definition, project.state);
    const auto definition = maker::definition_bytes(checked.definition);
    const auto state = loom::serialize(checked.state);
    loom::Value bundle(project_schema());
    bundle.set("definition", loom::Cell::bytes(loom::Bytes(definition.begin(), definition.end())));
    bundle.set("state", loom::Cell::bytes(loom::Bytes(state.begin(), state.end())));
    return loom::serialize(bundle);
}

inline Project read_project(std::string_view bytes) {
    const auto envelope = loom::admit(loom::parse(bytes), project_schema());
    if (!envelope) throw std::invalid_argument(envelope.first_error().message());
    const auto& definition = envelope.value().get("definition")->as_bytes();
    auto admitted = maker::read_definition(std::string_view(
        reinterpret_cast<const char*>(definition.data()), definition.size()));
    if (!admitted) throw std::invalid_argument(admitted.reason);
    const auto& state = envelope.value().get("state")->as_bytes();
    auto value = loom::admit(loom::parse(std::string_view(
        reinterpret_cast<const char*>(state.data()), state.size())), admitted.definition.state);
    if (!value) throw std::invalid_argument(value.first_error().message());
    return {std::move(admitted.definition), std::move(value).value()};
}

inline Project open_project(const std::string& path) {
    const auto bytes = maker::read_file(path);
    if (!bytes) throw std::runtime_error(bytes.reason);
    return read_project(bytes.bytes);
}

inline void save_project(const std::string& path, const Project& project) {
    const auto bytes = project_bytes(project);
    if (bytes.size() > maker::kMaxFileBytes)
        throw std::runtime_error("project exceeds the 1 MiB file limit");
    const auto error = maker::write_file(path, bytes);
    if (!error.empty()) throw std::runtime_error(error);
}

inline maker::Definition definition_json(std::string_view bytes) {
    const auto value = loom::admit(loom::compat::parse(bytes), maker::definition_schema());
    if (!value) throw std::invalid_argument(value.first_error().message());
    auto definition = maker::admit_definition(value.value());
    if (!definition) throw std::invalid_argument(definition.reason);
    return std::move(definition.definition);
}

} // namespace zengine::flow
#endif
