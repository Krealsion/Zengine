// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_MESSAGE_DRAFT_TRANSFER_HPP
#define ZENGINE_MESSAGE_DRAFT_TRANSFER_HPP
#include "message-draft/library.hpp"

namespace zengine::message_draft {
// Complete transport shapes around the existing partial-data codec. These are data,
// not relaxed versions of the enclosed command and not execution permissions.
inline std::shared_ptr<const loom::Schema> stored_draft_schema() {
    static const auto s = loom::SchemaBuilder("zengine.message_draft.StoredDraft", 1)
        .field("library", loom::Kind::Bytes).build();
    return s;
}
inline std::shared_ptr<const loom::Schema> path_step_schema() {
    static const auto s = loom::SchemaBuilder("zengine.message_draft.PathStep", 1)
        .field("field", loom::Kind::Text, false).field("index", loom::Kind::Int, false).build();
    return s;
}
inline std::shared_ptr<const loom::Schema> field_value_schema() {
    static const auto s = loom::SchemaBuilder("zengine.message_draft.FieldValue", 1)
        .field("library", loom::Kind::Bytes).list("path", loom::type_message(path_step_schema())).build();
    return s;
}
inline bool is_stored_draft(const loom::Value& value) {
    return loom::same_identity(value.schema(), *stored_draft_schema());
}
inline bool is_field_value(const loom::Value& value) {
    return loom::same_identity(value.schema(), *field_value_schema());
}
inline loom::Value store_draft(std::string title, const Draft& draft) {
    Library library;
    library.put(std::move(title), draft);
    loom::Value value(stored_draft_schema());
    value.set("library", detail::byte_cell(library_bytes(library)));
    return value;
}
namespace detail {
inline Preset single_draft(const loom::Value& value, const loom::Schema& expected,
                           const loom::Registry* current) {
    const auto admitted = loom::admit(value, expected);
    if (!admitted) throw std::invalid_argument(admitted.first_error().message());
    const auto library = read_library(byte_view(*admitted.value().get("library")), current);
    if (library.entries().size() != 1) throw std::invalid_argument("transfer needs exactly one draft");
    return library.entries().front();
}
// Keep only ancestors of the selected cell. A list ancestor has one element and
// its path index becomes zero; unrelated sibling data never rides with the field.
inline loom::Cell prune(const loom::Cell& cell, const loom::TypeRef& type,
                        Path& path, std::size_t at) {
    depth_check(at);
    if (at == path.size()) return clone_cell(cell, 0);
    if (const auto* name = std::get_if<std::string>(&path[at])) {
        if (type.kind != loom::Kind::Message) throw std::invalid_argument("field path expects Message");
        const auto* field = type.message->find(*name);
        const auto* child = cell.as_message()->get(*name);
        if (!field || !child) throw std::invalid_argument("selected field is absent");
        loom::Value result(type.message);
        result.set(*name, prune(*child, field->type, path, at + 1));
        return loom::Cell::message(std::move(result));
    }
    const auto index = std::get<std::size_t>(path[at]);
    if (type.kind != loom::Kind::List || index >= cell.as_list().size())
        throw std::invalid_argument("selected list item is absent");
    path[at] = std::size_t(0);
    return loom::Cell::list({prune(cell.as_list()[index], *type.element, path, at + 1)});
}
} // namespace detail
inline Preset read_draft(const loom::Value& value, const loom::Registry* current = nullptr) {
    return detail::single_draft(value, *stored_draft_schema(), current);
}
inline loom::Value grab_field(const Draft& source, Path path) {
    if (path.empty() || !source.get(path)) throw std::invalid_argument("select a present field to grab");
    const auto selected = detail::prune(loom::Cell::message(source.snapshot()),
                                        loom::type_message(source.schema()), path, 0);
    Library library;
    library.put("field", Draft(*selected.as_message()));
    loom::Value result(field_value_schema());
    result.set("library", detail::byte_cell(library_bytes(library)));
    loom::Cell::Array steps;
    for (const auto& part : path) {
        loom::Value step(path_step_schema());
        if (const auto* name = std::get_if<std::string>(&part)) step.set("field", loom::Cell::text(*name));
        else step.set("index", loom::Cell::integer(static_cast<std::int64_t>(std::get<std::size_t>(part))));
        steps.push_back(loom::Cell::message(std::move(step)));
    }
    result.set("path", loom::Cell::list(std::move(steps)));
    return result;
}
struct FieldValue { loom::TypeRef type; loom::Cell cell; };
inline FieldValue read_field(const loom::Value& value) {
    const auto entry = detail::single_draft(value, *field_value_schema(), nullptr);
    Path path;
    for (const auto& part : value.get("path")->as_list()) {
        const auto& step = *part.as_message();
        const auto* field = step.get("field");
        const auto* index = step.get("index");
        if ((field != nullptr) == (index != nullptr)) throw std::invalid_argument("ambiguous field path step");
        if (field) path.emplace_back(field->as_text());
        else {
            if (index->as_int() < 0) throw std::invalid_argument("negative field path index");
            path.emplace_back(static_cast<std::size_t>(index->as_int()));
        }
        detail::depth_check(path.size());
    }
    const auto* cell = entry.draft.get(path);
    if (!cell) throw std::invalid_argument("selected field is absent");
    return {entry.draft.type(path), detail::clone_cell(*cell, 0)};
}
inline void require_type(const loom::TypeRef& expected, const loom::TypeRef& actual,
                         std::size_t depth = 0) {
    detail::depth_check(depth);
    if (expected.kind != actual.kind) throw std::invalid_argument("field kinds do not agree");
    if (expected.kind == loom::Kind::List) require_type(*expected.element, *actual.element, depth + 1);
    if (expected.kind == loom::Kind::Message) {
        if (!loom::same_identity(*expected.message, *actual.message))
            throw std::invalid_argument("field message schemas do not agree");
        loom::Registry agreement;
        std::vector<std::shared_ptr<const loom::Schema>> roots;
        loom::collect_referenced(*expected.message, roots); roots.push_back(expected.message);
        loom::collect_referenced(*actual.message, roots); roots.push_back(actual.message);
        auto claim = agreement.claim(roots);
    }
}
} // namespace zengine::message_draft
#endif
