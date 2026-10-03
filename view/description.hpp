// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_DESCRIPTION_HPP
#define ZENGINE_VIEW_DESCRIPTION_HPP

// A view described as data: its size and its elements placed inside it in whole pixels, the
// field each label shows, and the intent each control says. It holds no business value and no
// resolved geometry, and its saved bytes are `zengine.view.Description`, refused by version
// number before a field is read. Law: agents/view.md. Reference: docs/reference/view.md.

#include "maker/files.hpp"
#include "surface/vocabulary.hpp"

#include <zen/gate.hpp>
#include <zen/kernel/schema_codec.hpp>
#include <zen/registry.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace zengine::view {

/// The word a description file says it is.
inline constexpr const char* kFormat = "zengine-view-description";
/// The description format's version, carried inside the value and as its envelope's version:
/// the one this build writes. It reads version 1 too, a description without a size.
inline constexpr std::int64_t kFormatVersion = 2;

/// A view's name is its office and its intents' namespace, and Workshop lists it as a pane.
inline constexpr std::size_t kMaxNameBytes = 32;
inline constexpr std::size_t kMaxElements = 64;
inline constexpr std::size_t kMaxIdBytes = 32;
inline constexpr std::size_t kMaxLabelBytes = 64;
/// The longest text a number field holds.
inline constexpr std::size_t kMaxFieldTextBytes = 32;
/// A place or a size in whole pixels, `surface::kCanvasCellPx` to a cell, at most this.
inline constexpr std::int64_t kMaxPixels = 8192;
/// A view's own size in whole pixels: at least room for a few columns and a notice row, and at
/// most as far as any element may reach, so every element a place allows fits in some size.
inline constexpr std::int64_t kMinWidthPx = 120;
inline constexpr std::int64_t kMinHeightPx = 48;
inline constexpr std::int64_t kMaxSizePx = 2 * kMaxPixels;
/// The rows the notice has beneath the view's size: lines of the medium's text, so no element,
/// which sits inside the size, is ever under it.
inline constexpr std::size_t kNoticeRows = 3;

enum class Kind { label, number, button };

inline const char* kind_word(Kind kind) {
    switch (kind) {
    case Kind::label: return "label";
    case Kind::number: return "number";
    case Kind::button: return "button";
    }
    return "label";
}

inline std::optional<Kind> kind_of(std::string_view word) {
    if (word == "label") return Kind::label;
    if (word == "number") return Kind::number;
    if (word == "button") return Kind::button;
    return std::nullopt;
}

/// One element: what it is, what it says, and where it sits in whole pixels. A number field's
/// `text` is the text it starts with; the other kinds hold none.
struct Element {
    std::string id;
    Kind kind = Kind::label;
    std::string label;
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::string text;
};

/// A label that shows one field of a view-model shape: what the view is told.
struct Shows {
    std::string element;
    std::shared_ptr<const loom::Schema> shape;
    std::string field;
};

/// One intent field and the number field its value comes from.
struct IntentField {
    std::string field;
    std::string element;
};

/// What a control says when it is used: an intent shape, each field from a field of the view.
struct Intent {
    std::string control;
    std::shared_ptr<const loom::Schema> shape;
    std::vector<IntentField> fields;
};

struct Description {
    std::string name;
    /// The view's size in whole pixels: every element sits inside it, and its notice rows lie
    /// beneath it, in the medium's lines.
    std::int64_t width = 0, height = 0;
    std::vector<Element> elements;
    std::vector<Shows> shows;
    std::vector<Intent> intents;

    const Element* element(std::string_view id) const {
        for (const auto& e : elements)
            if (e.id == id) return &e;
        return nullptr;
    }
    const Shows* shown(std::string_view element) const {
        for (const auto& s : shows)
            if (s.element == element) return &s;
        return nullptr;
    }
    const Intent* intent(std::string_view control) const {
        for (const auto& i : intents)
            if (i.control == control) return &i;
        return nullptr;
    }
    /// The view-model shapes the view is told, each once, in the order its labels name them.
    std::vector<std::shared_ptr<const loom::Schema>> told() const {
        std::vector<std::shared_ptr<const loom::Schema>> out;
        for (const auto& s : shows)
            if (std::none_of(out.begin(), out.end(), [&](const auto& have) {
                    return loom::same_identity(*have, *s.shape);
                }))
                out.push_back(s.shape);
        return out;
    }
    /// The intent shapes the view says, in its controls' order.
    std::vector<std::shared_ptr<const loom::Schema>> says() const {
        std::vector<std::shared_ptr<const loom::Schema>> out;
        for (const auto& i : intents) out.push_back(i.shape);
        return out;
    }
};

/// Do two descriptions register the same participant: one name, the same shapes told and said?
/// Labels, places, sizes and starting text are not part of it.
inline bool same_shapes(const Description& a, const Description& b) {
    if (a.name != b.name) return false;
    const auto same = [](const auto& left, const auto& right) {
        if (left.size() != right.size()) return false;
        for (const auto& shape : left)
            if (std::none_of(right.begin(), right.end(),
                             [&](const auto& other) { return loom::same_identity(*shape, *other); }))
                return false;
        return true;
    };
    return same(a.told(), b.told()) && same(a.says(), b.says());
}

/// THE SIZE A DESCRIPTION WITHOUT ONE TAKES: the pane it was asked for before a view had a size --
/// a row below its lowest element and its notice rows, two columns past its rightmost, at least 4
/// rows by 40 columns and at most 60 by 200 -- less the notice rows, which now lie beneath the
/// size, and never less than its elements reach.
inline std::pair<std::int64_t, std::int64_t> fitting_size(const Description& d) {
    std::int64_t right = 0, bottom = 0;
    for (const auto& e : d.elements) {
        right = std::max(right, e.x + e.w);
        bottom = std::max(bottom, e.y + e.h);
    }
    const auto cell = surface::kCanvasCellPx;
    const auto cells = [&](std::int64_t px) { return (px + cell - 1) / cell; };
    const auto columns = std::clamp<std::int64_t>(cells(right) + 2, 40, 200);
    const auto rows = std::clamp<std::int64_t>(cells(bottom) + static_cast<std::int64_t>(kNoticeRows) + 1, 4, 60);
    return {std::clamp<std::int64_t>(std::max(columns * cell, right), kMinWidthPx, kMaxSizePx),
            std::clamp<std::int64_t>(std::max((rows - static_cast<std::int64_t>(kNoticeRows)) * cell, bottom),
                                     kMinHeightPx, kMaxSizePx)};
}

// ---- the saved format ------------------------------------------------------------------------

inline std::shared_ptr<const loom::Schema> element_schema() {
    static const auto s = loom::SchemaBuilder("zengine.view.Element", 1)
                              .field("id", loom::Kind::Text)
                              .field("kind", loom::Kind::Text)
                              .field("label", loom::Kind::Text)
                              .field("x", loom::Kind::Int)
                              .field("y", loom::Kind::Int)
                              .field("w", loom::Kind::Int)
                              .field("h", loom::Kind::Int)
                              .field("text", loom::Kind::Text, /*required=*/false)
                              .build();
    return s;
}

inline std::shared_ptr<const loom::Schema> shows_schema() {
    static const auto s = loom::SchemaBuilder("zengine.view.Shows", 1)
                              .field("element", loom::Kind::Text)
                              .field("shape_name", loom::Kind::Text)
                              .field("shape_version", loom::Kind::Int)
                              .field("field", loom::Kind::Text)
                              .build();
    return s;
}

inline std::shared_ptr<const loom::Schema> intent_field_schema() {
    static const auto s = loom::SchemaBuilder("zengine.view.IntentField", 1)
                              .field("field", loom::Kind::Text)
                              .field("element", loom::Kind::Text)
                              .build();
    return s;
}

inline std::shared_ptr<const loom::Schema> intent_schema() {
    static const auto s = loom::SchemaBuilder("zengine.view.Intent", 1)
                              .field("control", loom::Kind::Text)
                              .field("shape_name", loom::Kind::Text)
                              .field("shape_version", loom::Kind::Int)
                              .list("fields", loom::type_message(intent_field_schema()))
                              .build();
    return s;
}

/// The description artifact: its size, and every shape it tells or says listed once in
/// `shapes`, after the shapes they nest in `referenced`.
inline std::shared_ptr<const loom::Schema> description_schema() {
    static const auto s =
        loom::SchemaBuilder("zengine.view.Description", static_cast<std::uint32_t>(kFormatVersion))
            .field("format", loom::Kind::Text)
            .field("format_version", loom::Kind::Int)
            .field("name", loom::Kind::Text)
            .field("width", loom::Kind::Int)
            .field("height", loom::Kind::Int)
            .list("referenced", loom::type_message(loom::schema_desc_schema()), /*required=*/false)
            .list("shapes", loom::type_message(loom::schema_desc_schema()))
            .list("elements", loom::type_message(element_schema()))
            .list("shows", loom::type_message(shows_schema()))
            .list("intents", loom::type_message(intent_schema()))
            .build();
    return s;
}

/// The first description artifact, read and never written: no size, which its reader takes
/// from `fitting_size`.
inline std::shared_ptr<const loom::Schema> description_schema_v1() {
    static const auto s =
        loom::SchemaBuilder("zengine.view.Description", 1)
            .field("format", loom::Kind::Text)
            .field("format_version", loom::Kind::Int)
            .field("name", loom::Kind::Text)
            .list("referenced", loom::type_message(loom::schema_desc_schema()), /*required=*/false)
            .list("shapes", loom::type_message(loom::schema_desc_schema()))
            .list("elements", loom::type_message(element_schema()))
            .list("shows", loom::type_message(shows_schema()))
            .list("intents", loom::type_message(intent_schema()))
            .build();
    return s;
}

/// A description the view host will register, or why not, in one sentence.
struct Admitted {
    bool ok = false;
    std::string reason;
    Description description;

    static Admitted no(std::string why) {
        Admitted a;
        a.reason = std::move(why);
        return a;
    }
    explicit operator bool() const noexcept { return ok; }
};

namespace detail {

inline bool printable(std::string_view text) {
    return std::all_of(text.begin(), text.end(), [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return byte >= 32 && byte <= 126;
    });
}

/// An element id: a field name an intent can carry.
inline bool identifier(std::string_view id) {
    if (id.empty() || id.size() > kMaxIdBytes) return false;
    const auto first = static_cast<unsigned char>(id.front());
    if (!(std::isalpha(first) || first == '_')) return false;
    return std::all_of(id.begin(), id.end(), [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return std::isalnum(byte) || byte == '_';
    });
}

/// A view's name: an office and a namespace, so letters, digits, `_`, `-` and inner dots.
inline bool view_name(std::string_view name) {
    if (name.empty() || name.size() > kMaxNameBytes || name.front() == '.' || name.back() == '.')
        return false;
    return std::all_of(name.begin(), name.end(), [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return std::isalnum(byte) || byte == '_' || byte == '-' || byte == '.';
    });
}

inline std::string label_of(const loom::Schema& shape) {
    return "`" + shape.name() + " v" + std::to_string(shape.version()) + "`";
}

inline bool scalar(loom::Kind kind) {
    return kind == loom::Kind::Int || kind == loom::Kind::Float || kind == loom::Kind::Text ||
           kind == loom::Kind::Bool;
}

} // namespace detail

/// THE RULES A DESCRIPTION KEEPS, the same at every door: a usable name; a size from
/// `kMinWidthPx` by `kMinHeightPx` to `kMaxSizePx` each way; at most `kMaxElements` elements of
/// distinct identifier ids, printable labels and places inside `kMaxPixels`, each inside the
/// size; each
/// label showing one scalar field of a shape, two shapes of one name and version agreeing; each
/// intent on a button, inside the view's name, every field an `Int` from a number field.
inline std::string problem(const Description& d) {
    if (!detail::view_name(d.name))
        return "a view's name is 1 to " + std::to_string(kMaxNameBytes) +
               " bytes of letters, digits, `_`, `-` and inner dots; `" + d.name + "` is not";
    if (d.width < kMinWidthPx || d.height < kMinHeightPx || d.width > kMaxSizePx || d.height > kMaxSizePx)
        return "a view's size is whole pixels from " + std::to_string(kMinWidthPx) + " by " +
               std::to_string(kMinHeightPx) + " to " + std::to_string(kMaxSizePx) + " by " +
               std::to_string(kMaxSizePx) + "; " + std::to_string(d.width) + " by " +
               std::to_string(d.height) + " is not";
    if (d.elements.size() > kMaxElements)
        return "a view holds at most " + std::to_string(kMaxElements) + " elements";
    for (std::size_t i = 0; i < d.elements.size(); ++i) {
        const auto& e = d.elements[i];
        if (!detail::identifier(e.id))
            return "an element's id is 1 to " + std::to_string(kMaxIdBytes) +
                   " bytes of letters, digits and `_`, not beginning with a digit; `" + e.id +
                   "` is not";
        for (std::size_t j = 0; j < i; ++j)
            if (d.elements[j].id == e.id) return "two elements are both `" + e.id + "`";
        if (e.label.size() > kMaxLabelBytes || !detail::printable(e.label))
            return "`" + e.id + "`'s label is at most " + std::to_string(kMaxLabelBytes) +
                   " printable ASCII bytes";
        if (e.x < 0 || e.y < 0 || e.w < 1 || e.h < 1 || e.x > kMaxPixels || e.y > kMaxPixels ||
            e.w > kMaxPixels || e.h > kMaxPixels)
            return "`" + e.id + "` sits at a place and a size of whole pixels, each from 0 (a size "
                   "from 1) to " + std::to_string(kMaxPixels);
        if (e.x + e.w > d.width || e.y + e.h > d.height)
            return "`" + e.id + "` reaches to " + std::to_string(e.x + e.w) + "," +
                   std::to_string(e.y + e.h) + ", past the view's size of " + std::to_string(d.width) +
                   " by " + std::to_string(d.height);
        if (e.kind != Kind::number && !e.text.empty())
            return "`" + e.id + "` is a " + kind_word(e.kind) + "; only a number field starts with text";
        if (e.text.size() > kMaxFieldTextBytes || !detail::printable(e.text))
            return "`" + e.id + "` starts with at most " + std::to_string(kMaxFieldTextBytes) +
                   " printable ASCII bytes";
    }
    std::vector<std::shared_ptr<const loom::Schema>> seen;
    const auto agree = [&](const std::shared_ptr<const loom::Schema>& shape) -> std::string {
        for (const auto& have : seen)
            if (have->name() == shape->name() && have->version() == shape->version() &&
                !loom::same_identity(*have, *shape))
                return "two shapes are both " + detail::label_of(*shape) + " and differ";
        seen.push_back(shape);
        return {};
    };
    for (std::size_t i = 0; i < d.shows.size(); ++i) {
        const auto& s = d.shows[i];
        const auto* e = d.element(s.element);
        if (!e) return "a shown field names `" + s.element + "`, which the view does not hold";
        if (e->kind != Kind::label)
            return "`" + s.element + "` is a " + kind_word(e->kind) + "; only a label shows a field";
        for (std::size_t j = 0; j < i; ++j)
            if (d.shows[j].element == s.element) return "`" + s.element + "` shows two fields";
        const auto* field = s.shape->find(s.field);
        if (!field)
            return "`" + s.element + "` shows `" + s.field + "`, which " +
                   detail::label_of(*s.shape) + " does not declare";
        if (!detail::scalar(field->type.kind))
            return "`" + s.element + "` shows `" + s.field + "`, a " +
                   loom::name_of(field->type.kind) + "; a label shows an Int, Float, Text or Bool";
        if (auto why = agree(s.shape); !why.empty()) return why;
    }
    const auto prefix = d.name + ".";
    for (std::size_t i = 0; i < d.intents.size(); ++i) {
        const auto& in = d.intents[i];
        const auto* control = d.element(in.control);
        if (!control) return "an intent names `" + in.control + "`, which the view does not hold";
        if (control->kind != Kind::button)
            return "`" + in.control + "` is a " + kind_word(control->kind) + "; only a button says an intent";
        for (std::size_t j = 0; j < i; ++j) {
            if (d.intents[j].control == in.control) return "`" + in.control + "` says two intents";
            if (d.intents[j].shape->name() == in.shape->name())
                return "two intents are both `" + in.shape->name() + "`";
        }
        if (in.shape->name().size() <= prefix.size() || in.shape->name().rfind(prefix, 0) != 0)
            return "the intent " + detail::label_of(*in.shape) +
                   " is outside the view's name; its name must begin `" + prefix + "`";
        if (in.fields.size() != in.shape->fields().size())
            return "every field of " + detail::label_of(*in.shape) + " comes from a field of the view";
        for (const auto& f : in.shape->fields()) {
            const auto from = std::find_if(in.fields.begin(), in.fields.end(),
                                           [&](const auto& source) { return source.field == f.name; });
            if (from == in.fields.end())
                return "`" + f.name + "` of " + detail::label_of(*in.shape) +
                       " comes from no field of the view";
            const auto* source = d.element(from->element);
            if (!source || source->kind != Kind::number)
                return "`" + f.name + "` of " + detail::label_of(*in.shape) +
                       " comes from `" + from->element + "`, which is no number field";
            if (f.type.kind != loom::Kind::Int || !f.required)
                return "`" + f.name + "` of " + detail::label_of(*in.shape) +
                       " is filled from a number field, so it is a required Int";
        }
        if (auto why = agree(in.shape); !why.empty()) return why;
    }
    return {};
}

/// The description as the value its file holds.
inline loom::Value encode(const Description& d) {
    loom::Value v(description_schema());
    v.set("format", loom::Cell::text(kFormat));
    v.set("format_version", loom::Cell::integer(kFormatVersion));
    v.set("name", loom::Cell::text(d.name));
    v.set("width", loom::Cell::integer(d.width));
    v.set("height", loom::Cell::integer(d.height));
    std::vector<std::shared_ptr<const loom::Schema>> shapes = d.told();
    for (const auto& s : d.says()) shapes.push_back(s);
    std::vector<std::shared_ptr<const loom::Schema>> referenced;
    for (const auto& s : shapes) loom::collect_referenced(*s, referenced);
    const auto descriptors = [](const std::vector<std::shared_ptr<const loom::Schema>>& list) {
        loom::Cell::Array cells;
        for (const auto& s : list) cells.push_back(loom::Cell::message(loom::encode_schema(*s)));
        return loom::Cell::list(std::move(cells));
    };
    if (!referenced.empty()) v.set("referenced", descriptors(referenced));
    v.set("shapes", descriptors(shapes));
    loom::Cell::Array elements;
    for (const auto& e : d.elements) {
        loom::Value ev(element_schema());
        ev.set("id", loom::Cell::text(e.id));
        ev.set("kind", loom::Cell::text(kind_word(e.kind)));
        ev.set("label", loom::Cell::text(e.label));
        ev.set("x", loom::Cell::integer(e.x));
        ev.set("y", loom::Cell::integer(e.y));
        ev.set("w", loom::Cell::integer(e.w));
        ev.set("h", loom::Cell::integer(e.h));
        if (!e.text.empty()) ev.set("text", loom::Cell::text(e.text));
        elements.push_back(loom::Cell::message(std::move(ev)));
    }
    v.set("elements", loom::Cell::list(std::move(elements)));
    loom::Cell::Array shows;
    for (const auto& s : d.shows) {
        loom::Value sv(shows_schema());
        sv.set("element", loom::Cell::text(s.element));
        sv.set("shape_name", loom::Cell::text(s.shape->name()));
        sv.set("shape_version", loom::Cell::integer(static_cast<std::int64_t>(s.shape->version())));
        sv.set("field", loom::Cell::text(s.field));
        shows.push_back(loom::Cell::message(std::move(sv)));
    }
    v.set("shows", loom::Cell::list(std::move(shows)));
    loom::Cell::Array intents;
    for (const auto& in : d.intents) {
        loom::Value iv(intent_schema());
        iv.set("control", loom::Cell::text(in.control));
        iv.set("shape_name", loom::Cell::text(in.shape->name()));
        iv.set("shape_version", loom::Cell::integer(static_cast<std::int64_t>(in.shape->version())));
        loom::Cell::Array fields;
        for (const auto& f : in.fields) {
            loom::Value fv(intent_field_schema());
            fv.set("field", loom::Cell::text(f.field));
            fv.set("element", loom::Cell::text(f.element));
            fields.push_back(loom::Cell::message(std::move(fv)));
        }
        iv.set("fields", loom::Cell::list(std::move(fields)));
        intents.push_back(loom::Cell::message(std::move(iv)));
    }
    v.set("intents", loom::Cell::list(std::move(intents)));
    return v;
}

/// The description's bytes, refused when it breaks a rule: nothing that would be refused is
/// ever written.
inline std::string description_bytes(const Description& d) {
    if (auto why = problem(d); !why.empty()) throw std::invalid_argument(why);
    return loom::serialize(encode(d));
}

/// ADMIT A DESCRIPTION VALUE that passed the gate at `description_schema()` or, for version 1,
/// `description_schema_v1()`: the format word and version, the shapes decoded with their closure
/// first, each element's kind, the size (for version 1, the one its elements fit), then
/// `problem`.
inline Admitted admit(const loom::Value& v) {
    try {
        if (v.get("format")->as_text() != kFormat)
            return Admitted::no("not a view description: it says it is `" +
                                v.get("format")->as_text() + "`");
        if (v.get("format_version")->as_int() != static_cast<std::int64_t>(v.schema().version()))
            return Admitted::no("a description whose version field says " +
                                std::to_string(v.get("format_version")->as_int()) +
                                " inside an envelope of version " +
                                std::to_string(v.schema().version()) + " is a forgery");
        Description d;
        d.name = v.get("name")->as_text();
        loom::Registry deps;
        loom::decode_referenced(v, deps);
        std::vector<std::shared_ptr<const loom::Schema>> shapes;
        for (const auto& c : v.get("shapes")->as_list())
            shapes.push_back(loom::decode_schema(*c.as_message(), deps));
        const auto shape = [&](const loom::Value& rec) -> std::shared_ptr<const loom::Schema> {
            const auto name = rec.get("shape_name")->as_text();
            const auto version = rec.get("shape_version")->as_int();
            for (const auto& s : shapes)
                if (s->name() == name && static_cast<std::int64_t>(s->version()) == version) return s;
            throw std::invalid_argument("the description names `" + name + " v" +
                                        std::to_string(version) + "`, which it does not carry");
        };
        for (const auto& c : v.get("elements")->as_list()) {
            const auto& rec = *c.as_message();
            Element e;
            e.id = rec.get("id")->as_text();
            const auto kind = kind_of(rec.get("kind")->as_text());
            if (!kind)
                return Admitted::no("`" + e.id + "` is a `" + rec.get("kind")->as_text() +
                                    "`; an element is a label, a number field or a button");
            e.kind = *kind;
            e.label = rec.get("label")->as_text();
            e.x = rec.get("x")->as_int();
            e.y = rec.get("y")->as_int();
            e.w = rec.get("w")->as_int();
            e.h = rec.get("h")->as_int();
            if (const auto* text = rec.get("text")) e.text = text->as_text();
            d.elements.push_back(std::move(e));
        }
        for (const auto& c : v.get("shows")->as_list()) {
            const auto& rec = *c.as_message();
            d.shows.push_back({rec.get("element")->as_text(), shape(rec), rec.get("field")->as_text()});
        }
        for (const auto& c : v.get("intents")->as_list()) {
            const auto& rec = *c.as_message();
            Intent in{rec.get("control")->as_text(), shape(rec), {}};
            for (const auto& f : rec.get("fields")->as_list())
                in.fields.push_back({f.as_message()->get("field")->as_text(),
                                     f.as_message()->get("element")->as_text()});
            d.intents.push_back(std::move(in));
        }
        if (v.schema().version() == 1) {
            std::tie(d.width, d.height) = fitting_size(d);
        } else {
            d.width = v.get("width")->as_int();
            d.height = v.get("height")->as_int();
        }
        if (auto why = problem(d); !why.empty()) return Admitted::no(why);
        Admitted a;
        a.ok = true;
        a.description = std::move(d);
        return a;
    } catch (const std::exception& e) {
        return Admitted::no(e.what());
    }
}

/// READ A DESCRIPTION: the envelope's claim first -- another shape, or a version this build does
/// not read, is refused by its number before a field is decoded -- then the gate at that
/// version's shape, then `admit`. Version 1 reads with the size its elements fit, and is written
/// again as the current version.
inline Admitted read_description(std::string_view bytes) {
    const loom::Unverified claim = loom::parse(bytes);
    if (!claim.well_formed()) return Admitted::no("these bytes are not a Zen value");
    if (claim.claimed_name() != description_schema()->name())
        return Admitted::no("not a view description: the bytes claim `" + claim.claimed_name() + "`");
    const bool first = claim.claimed_version() == 1;
    if (claim.claimed_version() != static_cast<std::uint32_t>(kFormatVersion) && !first)
        return Admitted::no("a view description of version " +
                            std::to_string(claim.claimed_version()) + "; this build reads versions 1 to " +
                            std::to_string(kFormatVersion) + " and converts no other");
    auto admitted = loom::admit(claim, first ? description_schema_v1() : description_schema());
    if (!admitted) return Admitted::no(admitted.first_error().message());
    return admit(admitted.value());
}

/// Open and save a description file through the maker file doors: bounded, and replaced whole.
inline Description open_description(const std::string& path) {
    const auto bytes = maker::read_file(path);
    if (!bytes) throw std::runtime_error(bytes.reason);
    auto admitted = read_description(bytes.bytes);
    if (!admitted) throw std::invalid_argument(admitted.reason);
    return std::move(admitted.description);
}

inline void save_description(const std::string& path, const Description& d) {
    const auto bytes = description_bytes(d);
    if (bytes.size() > maker::kMaxFileBytes)
        throw std::runtime_error("a view description exceeds the file limit");
    const auto error = maker::write_file(path, bytes);
    if (!error.empty()) throw std::runtime_error(error);
}

} // namespace zengine::view
#endif
