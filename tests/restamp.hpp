// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_RESTAMP_HPP
#define ZENGINE_TESTS_RESTAMP_HPP

// A VALUE AS AN OLDER WRITER WROTE IT: the bytes a reader of several versions must still admit,
// made from the current encoding rather than kept as a frozen file. Shared by every suite whose
// format reads an earlier version beside the one it writes.

#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <memory>
#include <utility>

namespace zengine::testing {

/// A value's fields written again under another version of its schema, nested messages and lists
/// of them under that version's nested schemas, by field name: what a writer of that version
/// would have sent. A field the target lacks is dropped; one it adds stays absent.
inline loom::Cell restamp_cell(const loom::Cell& cell, const loom::TypeRef& type);

inline loom::Value restamp(const loom::Value& written,
                           const std::shared_ptr<const loom::Schema>& version) {
    loom::Value out(version);
    for (const loom::Field& field : version->fields()) {
        if (written.schema().find(field.name) == nullptr) {
            continue;
        }
        if (const loom::Cell* cell = written.get(field.name); cell != nullptr) {
            out.set(field.name, restamp_cell(*cell, field.type));
        }
    }
    return out;
}

inline loom::Cell restamp_cell(const loom::Cell& cell, const loom::TypeRef& type) {
    if (type.kind == loom::Kind::Message) {
        return loom::Cell::message(restamp(*cell.as_message(), type.message));
    }
    if (type.kind == loom::Kind::List && type.element->kind == loom::Kind::Message) {
        loom::Cell::Array items;
        for (const loom::Cell& item : cell.as_list()) {
            items.push_back(restamp_cell(item, *type.element));
        }
        return loom::Cell::list(std::move(items));
    }
    return cell;
}

} // namespace zengine::testing

#endif // ZENGINE_TESTS_RESTAMP_HPP
