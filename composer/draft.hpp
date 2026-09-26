// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPOSER_DRAFT_HPP
#define ZENGINE_COMPOSER_DRAFT_HPP


#include "component/text_box.hpp"
#include "message-draft/transfer.hpp"

#include <zen/kind.hpp>
#include <zen/registry.hpp>
#include <zen/schema.hpp>
#include <zen/terminal/composer.hpp>
#include <zen/terminal/input_lex.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::composer {

enum class Composability { kScalar, kNoSpelling, kNotFlat };

inline Composability composability(loom::Kind k) noexcept {
    switch (k) {
    case loom::Kind::Int:
    case loom::Kind::Float:
    case loom::Kind::Text:
    case loom::Kind::Bool:
        return Composability::kScalar;
    case loom::Kind::Bytes:
        return Composability::kNoSpelling;
    case loom::Kind::Message:
    case loom::Kind::List:
        break;
    }
    return Composability::kNotFlat;
}

struct FieldDraft {
    bool present = false;
    zengine::component::TextBox value;
    std::optional<loom::Cell> typed;
};

struct MessageDraft {
    std::shared_ptr<const loom::Schema> schema;
    std::vector<FieldDraft> fields;
    /// The schema's own type spellings, derived once by `begin_draft` beside the fields: deriving
    /// them per row made projecting a form quadratic in the field count (measured on a 40-row
    /// body: 4 us for three fields, 1.15 ms for a hundred and twenty, on every keystroke). A
    /// schema is immutable and `begin_draft` the one door, so nothing drifts and nothing caches.
    loom::ShapeDesc desc;

    bool valid() const noexcept {
        return schema != nullptr && fields.size() == schema->fields().size() &&
               desc.fields.size() == fields.size();
    }
    std::size_t size() const noexcept { return fields.size(); }
    const loom::Field& field(std::size_t i) const { return schema->fields()[i]; }
    /// How this field's declared type is SPELLED -- the schema's own word for it
    /// (`Int`, `List<Bytes>`, `Message(Leaf v1)`), and the only thing this tool ever
    /// says about what a value MEANS.
    const std::string& type_of(std::size_t i) const { return desc.fields[i].type; }
};

inline MessageDraft begin_draft(std::shared_ptr<const loom::Schema> schema) {
    MessageDraft d;
    d.schema = std::move(schema);
    if (d.schema != nullptr) {
        d.fields.resize(d.schema->fields().size());
        d.desc = loom::describe_schema(*d.schema);
    }
    return d;
}

inline void cycle(FieldDraft& draft, loom::Kind kind) {
    if (kind != loom::Kind::Bool) {
        draft.present = !draft.present;
        return;
    }
    if (!draft.present) {
        draft.present = true;
        draft.value.set("false", 0);
        return;
    }
    if (draft.value.text() == "false") {
        draft.value.set("true", 0);
        return;
    }
    draft.present = false;
}

inline bool typeable(loom::Kind kind) noexcept {
    return composability(kind) == Composability::kScalar && kind != loom::Kind::Bool;
}

struct Snapshot {
    std::unique_ptr<loom::Registry> deps;
    std::vector<std::shared_ptr<const loom::Schema>> roots;

    bool decoded() const noexcept { return deps != nullptr; }
};

class SnapshotSource final : public loom::ComposeSource {
public:
    explicit SnapshotSource(const Snapshot& snapshot) : snapshot_(snapshot) {}

    std::shared_ptr<const loom::Schema> resolve_schema(std::string_view name,
                                                       std::uint32_t version) const override {
        for (const auto& r : snapshot_.roots) {
            if (r != nullptr && r->name() == name && r->version() == version) {
                return r;
            }
        }
        if (snapshot_.deps == nullptr) {
            return nullptr;
        }
        return snapshot_.deps->lookup(name, version);
    }

    std::optional<loom::Cell> resolve_ref(const loom::Ref& ref, std::string* error) const override {
        if (error != nullptr) {
            *error = "cannot read $" + ref.label + "." + ref.field +
                     " -- this pane holds no received messages to refer to";
        }
        return std::nullopt;
    }

private:
    const Snapshot& snapshot_;
};

inline std::vector<loom::Arg> args_of(const MessageDraft& draft) {
    std::vector<loom::Arg> args;
    if (!draft.valid()) {
        return args;
    }
    for (std::size_t i = 0; i < draft.fields.size(); ++i) {
        const loom::Field& f = draft.field(i);
        if (!draft.fields[i].present ||
            composability(f.type.kind) != Composability::kScalar) {
            continue;
        }
        loom::Arg a;
        a.name = f.name;
        // THE ONE DECISION THIS FILE MAKES. See the header comment: a Text field
        // says "these bytes are text" with a schema, which is what the command
        // grammar's quote says with a keyboard.
        a.value = loom::lex_value(draft.fields[i].value.text(),
                                  f.type.kind == loom::Kind::Text);
        args.push_back(std::move(a));
    }
    return args;
}

inline message_draft::Draft partial(const MessageDraft& draft) {
    if (!draft.valid()) throw std::invalid_argument("no message is being composed");
    message_draft::Draft value(draft.schema);
    for (std::size_t i = 0; i < draft.size(); ++i) {
        const auto& field = draft.field(i);
        const auto& edit = draft.fields[i];
        if (!edit.present) continue;
        if (edit.typed) value.set({field.name}, *edit.typed);
        else if (composability(field.type.kind) == Composability::kScalar)
            value.set_text({field.name}, edit.value.text());
    }
    return value;
}

inline loom::Composition compose(const Snapshot& snapshot, const MessageDraft& draft) {
    if (!draft.valid()) {
        loom::Composition c;
        c.error = "no message is being composed";
        return c;
    }
    (void)snapshot;
    loom::Composition c;
    c.schema = draft.schema;
    try {
        auto value = partial(draft);
        for (std::size_t i = 0; i < draft.size(); ++i) {
            const auto& f = draft.field(i);
            if (const auto* cell = value.get({f.name})) c.cells.emplace(f.name, *cell);
            else if (f.required) c.open_fields.push_back(draft.desc.fields[i]);
        }
        if (c.open_fields.empty()) {
            const auto admitted = value.admit();
            if (!admitted) throw std::invalid_argument(admitted.first_error().message());
            c.status = loom::Composition::Status::Ready;
        } else c.status = loom::Composition::Status::NeedsInput;
    } catch (const std::exception& e) { c.error = e.what(); }
    return c;
}

inline void put_field(MessageDraft& draft, std::size_t index, const loom::Cell& cell) {
    if (!draft.valid() || index >= draft.size()) throw std::invalid_argument("no destination field");
    message_draft::Draft checked(draft.schema);
    const auto& field = draft.field(index);
    checked.set({field.name}, cell);
    const auto owned = checked.snapshot();
    const auto* value = owned.get(field.name);
    FieldDraft fresh;
    fresh.present = true;
    if (composability(field.type.kind) == Composability::kScalar)
        fresh.value.set(message_draft::summary(value), 0);
    else fresh.typed = *value;
    draft.fields[index] = std::move(fresh);
}
inline MessageDraft from_message(std::shared_ptr<const loom::Schema> expected, const loom::Value& item) {
    const auto admission = loom::admit(item, *expected);
    if (!admission) throw std::invalid_argument(admission.first_error().message());
    auto result = begin_draft(std::move(expected));
    for (std::size_t i = 0; i < result.size(); ++i)
        if (const auto* cell = admission.value().get(result.field(i).name)) put_field(result, i, *cell);
    return result;
}
inline MessageDraft from_draft(std::shared_ptr<const loom::Schema> expected,
                              const message_draft::Draft& source) {
    if (!loom::same_identity(*expected, *source.schema()))
        throw std::invalid_argument("preset does not match the selected message shape");
    loom::Registry agreement;
    std::vector<std::shared_ptr<const loom::Schema>> closure;
    loom::collect_referenced(*expected, closure); closure.push_back(expected);
    loom::collect_referenced(*source.schema(), closure); closure.push_back(source.schema());
    auto claim = agreement.claim(closure);
    auto result = begin_draft(std::move(expected));
    for (std::size_t i = 0; i < result.size(); ++i)
        if (const auto* cell = source.value().get(result.field(i).name)) put_field(result, i, *cell);
    return result;
}
inline bool has_work(const MessageDraft& draft) {
    for (const auto& f : draft.fields) if (f.present || !f.value.text().empty() || f.typed) return true;
    return false;
}

} // namespace zengine::composer

#endif // ZENGINE_COMPOSER_DRAFT_HPP
