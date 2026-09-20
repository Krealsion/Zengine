// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_MESSAGE_DRAFT_LIBRARY_HPP
#define ZENGINE_MESSAGE_DRAFT_LIBRARY_HPP

#include "message-draft/draft.hpp"
#include "maker/files.hpp"
#include <zen/serialize.hpp>
#include <zen/weave/describe.hpp>
#include <algorithm>
#include <unordered_map>

namespace zengine::message_draft {

struct Preset {
    std::string title;
    Draft draft;
};

class Library {
public:
    const std::vector<Preset>& entries() const noexcept { return entries_; }
    Draft* find(std::string_view title) {
        for (auto& entry : entries_) if (entry.title == title) return &entry.draft;
        return nullptr;
    }
    const Draft* find(std::string_view title) const {
        for (const auto& entry : entries_) if (entry.title == title) return &entry.draft;
        return nullptr;
    }
    void put(std::string title, Draft draft) {
        if (title.empty()) throw std::invalid_argument("a saved draft needs a title");
        auto candidate = entries_;
        auto found = std::find_if(candidate.begin(), candidate.end(), [&](const auto& entry) {
            return entry.title == title;
        });
        if (found == candidate.end()) candidate.push_back({std::move(title), std::move(draft)});
        else found->draft = std::move(draft);
        loom::Registry agreement;
        std::vector<std::shared_ptr<const loom::Schema>> roots;
        for (const auto& entry : candidate) {
            loom::collect_referenced(*entry.draft.schema(), roots);
            roots.push_back(entry.draft.schema());
        }
        auto claim = agreement.claim(roots);
        entries_ = std::move(candidate);
    }
    bool erase(std::string_view title) {
        const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const auto& entry) {
            return entry.title == title;
        });
        if (found == entries_.end()) return false;
        entries_.erase(found);
        return true;
    }
private:
    std::vector<Preset> entries_;
};

namespace detail {
// The persistence projection relaxes presence only. Original schema descriptors travel
// separately. Nothing admits this projected shape to a runtime's actual message door.
class OptionalProjection {
public:
    std::shared_ptr<const loom::Schema> schema(const std::shared_ptr<const loom::Schema>& source,
                                              std::size_t depth = 0) {
        depth_check(depth);
        if (const auto found = schemas_.find(source.get()); found != schemas_.end()) return found->second;
        loom::SchemaBuilder builder(source->name(), source->version());
        for (const auto& field : source->fields()) builder.add({field.name, type(field.type, depth + 1), false});
        auto made = builder.build();
        schemas_.emplace(source.get(), made);
        return made;
    }
private:
    loom::TypeRef type(const loom::TypeRef& source, std::size_t depth) {
        depth_check(depth);
        if (source.kind == loom::Kind::Message) return loom::type_message(schema(source.message, depth));
        if (source.kind == loom::Kind::List) return loom::type_list(type(*source.element, depth + 1));
        return source;
    }
    std::unordered_map<const loom::Schema*, std::shared_ptr<const loom::Schema>> schemas_;
};

inline loom::Cell project_cell(const loom::Cell& cell, const loom::TypeRef& type, std::size_t depth);
inline loom::Value project_value(const loom::Value& value, const std::shared_ptr<const loom::Schema>& schema,
                                 std::size_t depth = 0) {
    depth_check(depth);
    loom::Value projected(schema);
    for (const auto& field : schema->fields())
        if (const auto* cell = value.get(field.name))
            projected.set(field.name, project_cell(*cell, field.type, depth + 1));
    return projected;
}
inline loom::Cell project_cell(const loom::Cell& cell, const loom::TypeRef& type, std::size_t depth) {
    depth_check(depth);
    if (type.kind == loom::Kind::Message)
        return loom::Cell::message(project_value(*cell.as_message(), type.message, depth));
    if (type.kind == loom::Kind::List) {
        loom::Cell::Array items;
        for (const auto& item : cell.as_list()) items.push_back(project_cell(item, *type.element, depth + 1));
        return loom::Cell::list(std::move(items));
    }
    return cell;
}
inline loom::Cell byte_cell(std::string_view bytes) {
    return loom::Cell::bytes(loom::Bytes(bytes.begin(), bytes.end()));
}
inline std::string_view byte_view(const loom::Cell& cell) {
    const auto& bytes = cell.as_bytes();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
inline void check_current(const std::vector<std::shared_ptr<const loom::Schema>>& roots,
                          const loom::Registry* current) {
    if (!current) return;
    std::vector<std::shared_ptr<const loom::Schema>> all;
    for (const auto& root : roots) {
        loom::collect_referenced(*root, all);
        all.push_back(root);
    }
    for (const auto& schema : all)
        if (const auto known = current->lookup(schema->name(), schema->version());
            known && !loom::same_identity(*known, *schema))
            throw loom::SchemaConflict(schema->name(), schema->version(), known->content_id(), schema->content_id());
}
} // namespace detail

inline std::shared_ptr<const loom::Schema> preset_schema() {
    static const auto schema = loom::SchemaBuilder("zengine.message_draft.Preset", 1)
        .field("title", loom::Kind::Text).field("draft", loom::Kind::Bytes).build();
    return schema;
}
inline std::shared_ptr<const loom::Schema> library_schema() {
    static const auto schema = loom::SchemaBuilder("zengine.message_draft.Library", 1)
        .message("schemas", loom::accepted_shapes_schema())
        .list("presets", loom::type_message(preset_schema())).build();
    return schema;
}

inline std::string library_bytes(const Library& library) {
    std::vector<std::shared_ptr<const loom::Schema>> roots;
    loom::Cell::Array presets;
    detail::OptionalProjection projection;
    for (const auto& entry : library.entries()) {
        // The same (name, version) may occur repeatedly, but never with conflicting shapes.
        roots.push_back(entry.draft.schema());
        const auto projected = detail::project_value(entry.draft.value(), projection.schema(entry.draft.schema()));
        const auto admitted = loom::admit(projected, projected.schema());
        if (!admitted) throw std::invalid_argument(admitted.first_error().message());
        loom::Value preset(preset_schema());
        preset.set("title", loom::Cell::text(entry.title));
        preset.set("draft", detail::byte_cell(loom::serialize(projected)));
        presets.push_back(loom::Cell::message(std::move(preset)));
    }
    loom::Registry agreement;
    std::vector<std::shared_ptr<const loom::Schema>> closure;
    for (const auto& root : roots) {
        loom::collect_referenced(*root, closure);
        closure.push_back(root);
    }
    auto claim = agreement.claim(closure);
    loom::Value envelope(library_schema());
    envelope.set("schemas", loom::Cell::message(loom::encode_accepted_shapes(roots)));
    envelope.set("presets", loom::Cell::list(std::move(presets)));
    const auto bytes = loom::serialize(envelope);
    if (bytes.size() > maker::kMaxFileBytes) throw std::invalid_argument("draft library exceeds the 1 MiB file limit");
    return bytes;
}

inline Library read_library(std::string_view bytes, const loom::Registry* current = nullptr) {
    if (bytes.size() > maker::kMaxFileBytes) throw std::invalid_argument("draft library exceeds the 1 MiB file limit");
    const auto envelope = loom::admit(loom::parse(bytes), library_schema());
    if (!envelope) throw std::invalid_argument(envelope.first_error().message());
    const auto& vocabulary = *envelope.value().get("schemas")->as_message();
    loom::Registry deps;
    loom::decode_accepted_referenced(vocabulary, deps);
    const auto roots = loom::decode_accepted_roots(vocabulary, deps);
    auto claim = deps.claim(roots);
    detail::check_current(roots, current);
    const auto& presets = envelope.value().get("presets")->as_list();
    if (presets.size() != roots.size()) throw std::invalid_argument("saved drafts and schema roots disagree");
    detail::OptionalProjection projection;
    Library out;
    for (std::size_t i = 0; i < presets.size(); ++i) {
        const auto& preset = *presets[i].as_message();
        const auto& title = preset.get("title")->as_text();
        if (out.find(title)) throw std::invalid_argument("duplicate saved draft title: " + title);
        const auto decoded = loom::admit(loom::parse(detail::byte_view(*preset.get("draft"))),
                                        projection.schema(roots[i]));
        if (!decoded) throw std::invalid_argument(title + ": " + decoded.first_error().message());
        out.put(title, Draft(detail::project_value(decoded.value(), roots[i])));
    }
    return out;
}

inline Library open_library(const std::string& path, const loom::Registry* current = nullptr) {
    const auto file = maker::read_file(path);
    if (!file) throw std::runtime_error(file.reason);
    return read_library(file.bytes, current);
}
inline void save_library(const std::string& path, const Library& library) {
    const auto bytes = library_bytes(library); // all validation before the old file is touched
    const auto error = maker::write_file(path, bytes);
    if (!error.empty()) throw std::runtime_error(error);
}

} // namespace zengine::message_draft
#endif
