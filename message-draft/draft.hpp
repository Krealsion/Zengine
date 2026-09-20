// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_MESSAGE_DRAFT_DRAFT_HPP
#define ZENGINE_MESSAGE_DRAFT_DRAFT_HPP

#include <zen/gate.hpp>
#include <zen/terminal/composer.hpp>
#include <zen/terminal/input_lex.hpp>

#include <functional>
#include <sstream>
#include <stdexcept>
#include <variant>

namespace zengine::message_draft {

// Paths are structured: punctuation in a schema's field name is not a path grammar.
using Step = std::variant<std::string, std::size_t>;
using Path = std::vector<Step>;

inline std::string path_label(const Path& path) {
    std::string out;
    for (const auto& step : path) {
        if (const auto* field = std::get_if<std::string>(&step)) {
            if (!out.empty()) out += ".";
            out += *field;
        } else out += "[" + std::to_string(std::get<std::size_t>(step)) + "]";
    }
    return out;
}

namespace detail {
inline constexpr std::size_t max_depth = 64;
inline void depth_check(std::size_t depth) {
    if (depth > max_depth) throw std::invalid_argument("draft exceeds 64 nesting levels");
}
inline loom::Cell clone_cell(const loom::Cell& cell, std::size_t depth);
inline loom::Value clone_value(const loom::Value& value, std::size_t depth = 0) {
    depth_check(depth);
    loom::Value out(value.schema_ptr());
    for (const auto& field : value.schema().fields())
        if (const auto* cell = value.get(field.name))
            out.set(field.name, clone_cell(*cell, depth + 1));
    return out;
}
inline loom::Cell clone_cell(const loom::Cell& cell, std::size_t depth) {
    depth_check(depth);
    if (cell.kind() == loom::Kind::Message) {
        if (!cell.as_message()) throw std::invalid_argument("null message in draft");
        return loom::Cell::message(clone_value(*cell.as_message(), depth));
    }
    if (cell.kind() == loom::Kind::List) {
        loom::Cell::Array out;
        for (const auto& item : cell.as_list()) out.push_back(clone_cell(item, depth + 1));
        return loom::Cell::list(std::move(out));
    }
    return cell;
}
inline void check_partial(const loom::Value& value) {
    for (const auto& error : loom::diagnose(value, value.schema()))
        if (error.kind != loom::ErrorKind::MissingField)
            throw std::invalid_argument(error.message());
}

// Rebuild the edited spine; no mutable nested Value escapes into another draft.
inline loom::Cell edited_cell(const loom::Cell& cell, const Path& path, std::size_t at,
                             const std::optional<loom::Cell>& replacement);
inline loom::Value edited_value(const loom::Value& value, const Path& path, std::size_t at,
                               const std::optional<loom::Cell>& replacement) {
    depth_check(at);
    const auto* name = std::get_if<std::string>(&path.at(at));
    if (!name || !value.schema().find(*name))
        throw std::invalid_argument("draft path does not name a declared field");
    loom::Value out(value.schema_ptr());
    for (const auto& field : value.schema().fields()) {
        const auto* cell = value.get(field.name);
        if (field.name != *name) {
            if (cell) out.set(field.name, *cell);
        } else if (at + 1 == path.size()) {
            if (replacement) out.set(field.name, *replacement);
        } else {
            if (!cell) throw std::invalid_argument("create the absent parent before editing its contents");
            out.set(field.name, edited_cell(*cell, path, at + 1, replacement));
        }
    }
    return out;
}
inline loom::Cell edited_cell(const loom::Cell& cell, const Path& path, std::size_t at,
                             const std::optional<loom::Cell>& replacement) {
    depth_check(at);
    if (cell.kind() == loom::Kind::Message && cell.as_message())
        return loom::Cell::message(edited_value(*cell.as_message(), path, at, replacement));
    const auto* index = std::get_if<std::size_t>(&path.at(at));
    if (cell.kind() != loom::Kind::List || !index || *index >= cell.as_list().size())
        throw std::invalid_argument("draft path does not name an existing list item");
    auto items = cell.as_list();
    if (at + 1 == path.size()) {
        if (!replacement) throw std::invalid_argument("a list item cannot be absent; erase it instead");
        items[*index] = *replacement;
    } else items[*index] = edited_cell(items[*index], path, at + 1, replacement);
    return loom::Cell::list(std::move(items));
}

class ScalarSource final : public loom::ComposeSource {
public:
    explicit ScalarSource(loom::TypeRef type)
        : schema(loom::SchemaBuilder("zengine.message_draft.Scalar", 1)
                     .add({"value", std::move(type), true}).build()) {}
    std::shared_ptr<const loom::Schema> resolve_schema(std::string_view name,
                                                       std::uint32_t version) const override {
        return name == schema->name() && version == schema->version() ? schema : nullptr;
    }
    std::optional<loom::Cell> resolve_ref(const loom::Ref&, std::string* error) const override {
        if (error) *error = "a saved value draft holds no received-message references";
        return std::nullopt;
    }
    std::shared_ptr<const loom::Schema> schema;
};
} // namespace detail

inline std::string summary(const loom::Cell* cell) {
    if (!cell) return "absent";
    switch (cell->kind()) {
    case loom::Kind::Int: return std::to_string(cell->as_int());
    case loom::Kind::Float: {
        std::ostringstream out;
        out.precision(17);
        out << cell->as_float();
        return out.str();
    }
    case loom::Kind::Text: return cell->as_text();
    case loom::Kind::Bool: return cell->as_bool() ? "true" : "false";
    case loom::Kind::Bytes: return std::to_string(cell->as_bytes().size()) + " bytes";
    case loom::Kind::Message: return "message";
    case loom::Kind::List: return std::to_string(cell->as_list().size()) + " items";
    }
    return {};
}

struct Row {
    Path path;
    loom::TypeRef type;
    bool present = false;
    bool required = false;
    std::string label;
    std::string summary;
};

class Draft {
public:
    explicit Draft(std::shared_ptr<const loom::Schema> schema)
        : value_(checked_schema(std::move(schema))) {}
    explicit Draft(const loom::Value& value) : value_(copy_checked(value)) {}
    Draft(const Draft& other) : value_(detail::clone_value(other.value_)) {}
    Draft& operator=(const Draft& other) {
        if (this != &other) value_ = detail::clone_value(other.value_);
        return *this;
    }
    Draft(Draft&&) noexcept = default;
    Draft& operator=(Draft&&) noexcept = default;

    const std::shared_ptr<const loom::Schema>& schema() const noexcept { return value_.schema_ptr(); }
    // Inspection only. Like Loom Value, shared nested messages are immutable to readers.
    const loom::Value& value() const noexcept { return value_; }
    loom::Value snapshot() const { return detail::clone_value(value_); }
    std::vector<loom::Error> errors() const { return loom::diagnose(value_, value_.schema()); }
    bool ready() const { return errors().empty(); }
    loom::Admission admit() const { return loom::admit(snapshot(), value_.schema(), loom::Report::Full); }
    loom::Admission admit(const loom::Schema& expected) const {
        return loom::admit(snapshot(), expected, loom::Report::Full);
    }

    const loom::Cell* get(const Path& path) const { return locate(path).cell; }
    loom::TypeRef type(const Path& path) const { return locate(path).type; }
    void set(const Path& path, loom::Cell value) { edit(path, std::move(value)); }
    void unset(const Path& path) { edit(path, std::nullopt); }
    void set_text(const Path& path, std::string_view text) {
        const auto field_type = type(path);
        if (field_type.kind == loom::Kind::Bytes || field_type.kind == loom::Kind::Message ||
            field_type.kind == loom::Kind::List)
            throw std::invalid_argument("this kind needs a typed value, not scalar text");
        const detail::ScalarSource source(field_type);
        loom::Arg arg;
        arg.name = "value";
        arg.value = loom::lex_value(std::string(text), field_type.kind == loom::Kind::Text);
        const auto composed = loom::compose_message(source, source.schema->name(), 1, {arg});
        if (!composed) throw std::invalid_argument(path_label(path) + ": " + composed.error);
        set(path, composed.cells.at("value"));
    }
    void create_message(const Path& path) {
        const auto field_type = type(path);
        if (field_type.kind != loom::Kind::Message) throw std::invalid_argument("field is not a Message");
        set(path, loom::Cell::message(loom::Value(field_type.message)));
    }
    void create_list(const Path& path) {
        if (type(path).kind != loom::Kind::List) throw std::invalid_argument("field is not a List");
        set(path, loom::Cell::list({}));
    }
    void append(const Path& path, loom::Cell value) {
        const auto cursor = locate(path);
        if (cursor.type.kind != loom::Kind::List || !cursor.cell)
            throw std::invalid_argument("create the list before appending to it");
        auto items = cursor.cell->as_list();
        items.push_back(std::move(value));
        set(path, loom::Cell::list(std::move(items)));
    }
    void erase(const Path& path, std::size_t index) {
        const auto cursor = locate(path);
        if (cursor.type.kind != loom::Kind::List || !cursor.cell || index >= cursor.cell->as_list().size())
            throw std::invalid_argument("no such list item");
        auto items = cursor.cell->as_list();
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
        set(path, loom::Cell::list(std::move(items)));
    }
    std::vector<Row> rows() const {
        std::vector<Row> out;
        std::function<void(const loom::TypeRef&, const loom::Cell*, Path, bool)> add;
        add = [&](const loom::TypeRef& field_type, const loom::Cell* cell, Path path, bool required) {
            detail::depth_check(path.size());
            out.push_back({path, field_type, cell != nullptr, required, path_label(path), summary(cell)});
            if (!cell) return;
            if (field_type.kind == loom::Kind::Message) {
                for (const auto& field : field_type.message->fields()) {
                    auto child = path;
                    child.emplace_back(field.name);
                    add(field.type, cell->as_message()->get(field.name), std::move(child), field.required);
                }
            } else if (field_type.kind == loom::Kind::List) {
                for (std::size_t i = 0; i < cell->as_list().size(); ++i) {
                    auto child = path;
                    child.emplace_back(i);
                    add(*field_type.element, &cell->as_list()[i], std::move(child), true);
                }
            }
        };
        for (const auto& field : value_.schema().fields())
            add(field.type, value_.get(field.name), {field.name}, field.required);
        return out;
    }

private:
    struct Cursor { loom::TypeRef type; const loom::Cell* cell; };
    Cursor locate(const Path& path) const {
        if (path.empty()) throw std::invalid_argument("draft path must name a field");
        detail::depth_check(path.size());
        auto field_type = loom::type_message(schema());
        const loom::Value* message = &value_;
        const loom::Cell* cell = nullptr;
        for (const auto& step : path) {
            if (const auto* name = std::get_if<std::string>(&step)) {
                if (field_type.kind != loom::Kind::Message)
                    throw std::invalid_argument("draft path expects a Message");
                const auto* field = field_type.message->find(*name);
                if (!field) throw std::invalid_argument("draft path names an unknown field: " + *name);
                if (!message) throw std::invalid_argument("create the absent parent before editing its contents");
                cell = message->get(*name);
                field_type = field->type;
            } else {
                const auto index = std::get<std::size_t>(step);
                if (field_type.kind != loom::Kind::List || !cell || index >= cell->as_list().size())
                    throw std::invalid_argument("draft path names no such list item");
                cell = &cell->as_list()[index];
                auto element = *field_type.element;
                field_type = std::move(element);
            }
            message = cell && field_type.kind == loom::Kind::Message ? cell->as_message().get() : nullptr;
        }
        return {std::move(field_type), cell};
    }
    void edit(const Path& path, std::optional<loom::Cell> replacement) {
        if (path.empty()) throw std::invalid_argument("draft path must name a field");
        auto candidate = detail::edited_value(value_, path, 0, replacement);
        value_ = copy_checked(candidate);
    }
    static std::shared_ptr<const loom::Schema> checked_schema(std::shared_ptr<const loom::Schema> schema) {
        if (!schema) throw std::invalid_argument("a draft needs a schema");
        return schema;
    }
    static loom::Value copy_checked(const loom::Value& value) {
        detail::check_partial(value);
        return detail::clone_value(value);
    }
    loom::Value value_;
};

} // namespace zengine::message_draft
#endif
