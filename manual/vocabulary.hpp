// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_MANUAL_VOCABULARY_HPP
#define ZENGINE_MANUAL_VOCABULARY_HPP

// The door an office answers for the shapes it accepts: a caller names a shape, and the office
// answers its page's section for it, from the manual compiled into it. Nothing keeps a table of
// shape documents: the office that accepts a shape is the one that says what it does.
// Reference: manual/docs/manual.md.

#include "manual/manual.hpp"

#include <zen/schema.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace zengine::manual {

/// Caller -> an office: what does a shape you accept do? `content_id` is the shape's content id as
/// the caller holds it (`0x` and sixteen hex digits, as `tests/shapes.txt` spells one), or empty to
/// ask for the office's own; the words describe one definition of the shape and are said against
/// it. Answered to the caller alone.
struct ShapeDocumentRequested {
    std::string shape;
    std::int64_t version = 0;
    std::string content_id;
    ZEN_SHAPE(ShapeDocumentRequested, 1, ZEN_FIELD(shape), ZEN_FIELD(version),
              ZEN_FIELD(content_id));
};

/// An office -> the caller: its page's section for the shape. `content_id` is the shape's content
/// id as this office accepts it and `page` the content id of the page the words come from;
/// `current` is false when the caller named another content id, whose shape these words do not
/// describe, and `said` then says so. A shape the office does not accept, or one its page has no
/// section for, is answered with `refusal` and no text.
struct ShapeDocumentShown {
    std::string shape;
    std::int64_t version = 0;
    std::string content_id;
    std::string page;
    std::string group;
    std::string text;
    bool current = false;
    std::string said;
    std::string refusal;
    ZEN_SHAPE(ShapeDocumentShown, 1, ZEN_FIELD(shape), ZEN_FIELD(version), ZEN_FIELD(content_id),
              ZEN_FIELD(page), ZEN_FIELD(group), ZEN_FIELD(text), ZEN_FIELD(current),
              ZEN_FIELD(said), ZEN_FIELD(refusal));
};

/// A shape's content id as the census spells it: `0x` and sixteen lowercase hex digits.
inline std::string content_id_text(loom::ContentId id) {
    char out[19];
    std::snprintf(out, sizeof out, "0x%016llx", static_cast<unsigned long long>(id));
    return std::string(out);
}

/// THE ANSWER TO `ask` FROM AN OFFICE whose gate is `accepted` and whose page is `page`: the
/// section for a shape the office accepts under that name and version, said against the content
/// id the caller named. A shape outside `zen.` that the office accepts and its page does not
/// describe is refused naming the page, as is one it does not accept.
template <class Schemas>
ShapeDocumentShown shape_document(const ShapeDocumentRequested& ask, const Schemas& accepted,
                                  const Manual& page) {
    ShapeDocumentShown out;
    out.shape = ask.shape;
    out.version = ask.version;
    out.page = std::string(page.content_id);
    for (const auto& schema : accepted) {
        if (schema->name() != ask.shape ||
            static_cast<std::int64_t>(schema->version()) != ask.version) {
            continue;
        }
        out.content_id = content_id_text(schema->content_id());
        const Section* section = page.find(ask.shape);
        if (section == nullptr || !(is_command_group(section->group) ||
                                    is_heard_group(section->group))) {
            out.refusal = "the page of " + std::string(page.office) + " has no section for `" +
                          ask.shape + "`";
            return out;
        }
        out.group = std::string(section->group);
        out.text = std::string(section->text);
        out.current = ask.content_id.empty() || ask.content_id == out.content_id;
        if (!out.current) {
            out.said = "out of date: these words describe `" + ask.shape + "` v" +
                       std::to_string(ask.version) + " as " + std::string(page.office) +
                       " accepts it, " + out.content_id + ", not " + ask.content_id;
        }
        return out;
    }
    out.refusal = std::string(page.office) + " does not accept `" + ask.shape + "` v" +
                  std::to_string(ask.version);
    return out;
}

} // namespace zengine::manual

#endif // ZENGINE_MANUAL_VOCABULARY_HPP
