// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_SHAPE_HPP
#define ZENGINE_FLOW_SHAPE_HPP

// The one model of an authored message shape: how a weaver names a message, adds a field and
// spells a field's type. Flow's graph draft and workbench declare messages through it, and the
// View Builder makes a view's intent through it, so a shape keeps these rules wherever it is
// authored. It edits no value; values are `message_draft::Draft`'s. A shape carried out of a pane
// travels with the shapes it nests (`carried`, `described`). Reference: docs/reference/flow.md.

#include "message-draft/library.hpp"

#include <zen/kernel/schema_codec.hpp>
#include <zen/registry.hpp>
#include <zen/schema.hpp>
#include <zen/weave/describe.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::flow::shape {

/// The scalar kinds a weaver spells by name.
inline loom::Kind scalar_kind(std::string_view name) {
    if (name == "Int") return loom::Kind::Int;
    if (name == "Bool") return loom::Kind::Bool;
    if (name == "Float") return loom::Kind::Float;
    if (name == "Text") return loom::Kind::Text;
    throw std::invalid_argument("scalar authoring accepts Int, Bool, Float, Text; "
                                "import-json carries full maker schemas");
}

/// A message's full name: one holding a `.` as written, any other inside `space`.
inline std::string qualified(const std::string& space, std::string name) {
    if (name.empty()) throw std::invalid_argument("a message needs a name");
    if (name.find('.') == std::string::npos) name = space + "." + name;
    return name;
}

/// Refuse `name` when a declared shape already holds it.
inline void refuse_taken(const std::vector<std::shared_ptr<const loom::Schema>>& declared,
                         const std::string& name) {
    for (const auto& shape : declared)
        if (shape->name() == name)
            throw std::invalid_argument("a message named " + name + " is already declared");
}

/// A new authored shape: its name, version 1, and fields of distinct, nonempty names.
inline std::shared_ptr<const loom::Schema> make(const std::string& name,
                                                std::vector<loom::Field> fields = {}) {
    if (name.empty()) throw std::invalid_argument("a message needs a name");
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].name.empty()) throw std::invalid_argument("a field needs a name");
        for (std::size_t j = 0; j < i; ++j)
            if (fields[j].name == fields[i].name)
                throw std::invalid_argument("message field name is empty or already used");
    }
    return loom::make_schema(name, 1, std::move(fields));
}

/// `shape` with one more field, at the same name and version: a field whose name is empty or
/// already used is refused.
inline std::shared_ptr<const loom::Schema> with_field(const loom::Schema& shape,
                                                      const std::string& name, loom::TypeRef type,
                                                      bool required = true) {
    if (name.empty() || shape.find(name))
        throw std::invalid_argument("message field name is empty or already used");
    auto fields = shape.fields();
    fields.push_back({name, std::move(type), required});
    return loom::make_schema(shape.name(), shape.version(), std::move(fields));
}

/// A field's type from its spelling: a scalar, `Bytes`, `List:<type>`, or `Message:<name>` for
/// a saved draft's shape in `library`.
inline loom::TypeRef type_named(std::string_view spelling,
                                const message_draft::Library& library) {
    std::size_t lists = 0, offset = 0;
    while (spelling.compare(offset, 5, "List:") == 0) {
        if (++lists > 64) throw std::invalid_argument("type exceeds 64 List nesting levels");
        offset += 5;
    }
    const auto base = spelling.substr(offset);
    loom::TypeRef result;
    if (base.starts_with("Message:")) {
        const auto* saved = library.find(base.substr(8));
        if (!saved)
            throw std::invalid_argument("Message:<saved draft name> needs an existing library schema");
        result = loom::type_message(saved->schema());
    } else if (base == "Bytes") {
        result = loom::type_of(loom::Kind::Bytes);
    } else {
        result = loom::type_of(scalar_kind(base));
    }
    while (lists > 0) {
        result = loom::type_list(std::move(result));
        --lists;
    }
    return result;
}

/// A SHAPE CARRIED OUT OF A PANE, as one value that holds it and every shape it nests: Loom's
/// pairing of roots with their closure (`zen.AcceptedShapes`), the shape its one root and what it
/// nests in `referenced`, in post-order.
inline loom::Value carried(const std::shared_ptr<const loom::Schema>& shape) {
    return loom::encode_accepted_shapes({shape});
}

/// Is `value` a description of a shape: a carried one, or a bare `zen.SchemaDesc`?
inline bool is_description(const loom::Value& value) {
    return loom::same_identity(value.schema(), *loom::accepted_shapes_schema()) ||
           loom::same_identity(value.schema(), *loom::schema_desc_schema());
}

/// THE SHAPE A DESCRIPTION NAMES, decoded with the shapes it carries and nothing else: never a
/// reader's registry or a live catalog. A bare `zen.SchemaDesc` carries nothing beside it, so it
/// resolves only a shape that nests nothing. Throws, in Loom's words, when it names a shape it
/// does not carry, and when a carried one holds other than one shape.
inline std::shared_ptr<const loom::Schema> described(const loom::Value& value) {
    loom::Registry carried_shapes;
    if (!loom::same_identity(value.schema(), *loom::accepted_shapes_schema()))
        return loom::decode_schema(value, carried_shapes);
    loom::decode_accepted_referenced(value, carried_shapes);
    const auto roots = loom::decode_accepted_roots(value, carried_shapes);
    if (roots.size() != 1)
        throw std::invalid_argument("a carried description holds one shape; this one holds " +
                                    std::to_string(roots.size()));
    return roots.front();
}

} // namespace zengine::flow::shape
#endif
