// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SAMPLE_PRESENTATION_HPP
#define ZENGINE_WORKSHOP_SAMPLE_PRESENTATION_HPP

// WHAT A SAMPLED VALUE LOOKS LIKE TO A PERSON (SOURCE-1) -- one pure function.
//
//     render_value(value)  ->  the logical lines a maker reads
//
// ---- WHY IT IS HERE AND NOT IN THE PANE ----------------------------------------
//
// Because the SCHEMA is here. `zengine-introspection` is a woven weave: its
// accept-set is closed at compile time, so a shape it has never heard of cannot be
// declared, and its image links no operator target at all. A sampled `loom::Value`
// claims whatever schema its Source authored, which nothing in that image can name
// in advance -- and raw bytes without schema custody explain no field to anybody.
// So the value is projected to prose WHERE THE SCHEMA STILL IS, and the prose
// crosses as ordinary wire data (workshop/sample_vocabulary.hpp).
//
// ---- WHY IT IS NOT `loom::compat::serialize` -----------------------------------
//
// That codec is total and public and would have cost nothing, and it is refused on
// a MEASUREMENT rather than on taste: it renders an Int as a JSON *string*
// (`"recipes":"6"`), Bytes as base64, and wraps both in envelope noise. Its own
// header calls it a debug codec. Adopting it would make a debug representation into
// product law -- a maker reading `"6"` cannot tell a count from a caption.
//
// ---- WHAT IT IS, EXACTLY -------------------------------------------------------
//
//     zengine.RecipeCatalog v1
//       catalog
//         source   "/home/maker/zen/workshop-recipes.json"
//         recipes  6
//
// The schema line says what the answer CLAIMS to be. Quotes say Text, bare digits
// say Int, indentation says the message boundary. Nothing in it depends on WHICH
// Source answered: there is no identity in this file, no table of known schemas, no
// registration and no dispatch beyond `loom::Kind`.
//
// ---- WHAT BOUNDS IT, AND WHY EVERY BOUND MARKS ITSELF ---------------------------
//
// A value may nest arbitrarily and a list may be arbitrarily long, and this
// function's answer becomes ROWS OF A PANE. So depth, list members and total lines
// are each bounded -- and every bound that fires SAYS SO, because an unmarked cut
// is the one failure a presenter must never commit: a maker reading four fields of
// a six-field message has been told something false about the message.
//
// ---- AND IT IS NOT AN INSPECTOR FRAMEWORK --------------------------------------
//
// One function, one consumer, no registry, no per-schema renderer, no policy
// object, no formatting options and no extension point. A SECOND independent
// consumer -- a Flow node preview, a maker-facing console -- is what would earn a
// shared presentation home, and on that day this moves there whole. Until then it
// is the Powers sample presenter and it is named after what it does.

#include <zen/kind.hpp>
#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace zengine::workshop {

// ---- The bounds, named where they are spent ------------------------------------

/// HOW DEEP A NESTED MESSAGE IS FOLLOWED. Four levels is more than either shipped
/// host Source needs (they are one and two) and enough that a maker meets the mark
/// rather than the wall.
inline constexpr std::int64_t kSampleMaxDepth = 4;

/// HOW MANY MEMBERS OF ONE LIST ARE SPELLED OUT before the rest are COUNTED. The
/// count is exact -- a list's length is known before a single member is rendered --
/// so this is a bound on the prose and never on the fact.
inline constexpr std::size_t kSampleMaxListItems = 8;

/// THE BACKSTOP ON THE WHOLE ANSWER. Depth and list bounds already bound a
/// well-shaped value; this bounds a wide one (a schema with hundreds of fields) and
/// is the only cut whose remainder cannot be counted -- so it is the only one whose
/// mark does not carry a number, and it says that in words.
inline constexpr std::size_t kSampleMaxLines = 64;

/// The mark, in the three plain characters every pane in this application spells a
/// cut with. It is duplicated from the panes deliberately: this file is host-side
/// and links nothing of theirs, and a shared constant would be a dependency edge
/// bought for three characters.
inline constexpr const char* kSampleElided = "...";

/// What a line says where the depth bound stopped the walk. It names the LIMIT
/// rather than the value, because the value is still there and is not empty.
inline constexpr const char* kSampleDeeper = "(deeper structure not shown)";

/// What a line says for a field the value does not carry. An optional field nobody
/// authored is an observed absence and reads as one.
inline constexpr const char* kSampleAbsent = "(absent)";

/// What the whole answer says when the line backstop fired.
inline constexpr const char* kSampleTooLong = "(... more, not shown)";

namespace detail {

/// A DOUBLE, WRITTEN SO IT READS BACK AS ITSELF. Fifteen significant digits first,
/// seventeen only if fifteen did not round-trip -- the standard shortest-exact
/// spelling, and the reason `1.5` is not printed as `1.500000` (which is what
/// `std::to_string` would have done) or as `1.5000000000000000` (which is what a
/// fixed seventeen would).
inline std::string spell_float(double v) {
    char buf[64];
    for (const int digits : {15, 17}) {
        const int n = std::snprintf(buf, sizeof(buf), "%.*g", digits, v);
        if (n <= 0 || static_cast<std::size_t>(n) >= sizeof(buf)) {
            break;
        }
        const std::string said(buf, static_cast<std::size_t>(n));
        if (digits == 17 || std::strtod(said.c_str(), nullptr) == v) {
            return said;
        }
    }
    return "0";
}

/// TEXT, QUOTED, AND SAFE TO PUT IN A ROW.
///
/// The quotes are what say `Text` -- they are how a maker tells `"6"` from `6`
/// without being told the schema. And the escaping is not decoration: these lines
/// become `SurfaceTextRow`s, whose contract is plain printable ASCII (one canvas
/// cell per byte), so a newline or a UTF-8 sequence in a sampled string would be
/// refused by Workshop and take the WHOLE update with it. Escaping is therefore the
/// honest rendering rather than a filter: every byte is still reported, in the one
/// notation a reader already knows.
inline std::string spell_text(const std::string& raw) {
    std::string out = "\"";
    for (const char c : raw) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte == '"' || byte == '\\') {
            out += '\\';
            out += c;
        } else if (byte >= 0x20u && byte < 0x7Fu) {
            out += c;
        } else {
            char hex[8];
            (void)std::snprintf(hex, sizeof(hex), "\\x%02X", static_cast<unsigned>(byte));
            out += hex;
        }
    }
    out += '"';
    return out;
}

/// `N` plus its noun, agreeing. One number a maker reads, written the way a maker
/// writes it -- `introspection::counted`'s rule, respelled here for the same reason
/// `kSampleElided` is.
inline std::string items_said(std::size_t n) {
    return std::to_string(n) + (n == 1 ? " item" : " items");
}

inline std::string indent_of(std::int64_t depth) {
    return std::string(static_cast<std::size_t>(depth) * 2u, ' ');
}

/// The walk's shared state: the lines so far, and whether the backstop has fired.
/// It is a struct rather than three reference parameters so that "have I run out of
/// lines" is asked in one place by everything that appends.
struct Sheet {
    std::vector<std::string> lines;
    bool cut = false;

    /// Append, unless the backstop has fired -- and fire it exactly once, with its
    /// own marked line, so the cut is visible rather than inferred from a length.
    void say(std::string line) {
        if (cut) {
            return;
        }
        if (lines.size() >= kSampleMaxLines) {
            cut = true;
            lines.push_back(std::string(kSampleElided) + " " + kSampleTooLong);
            return;
        }
        lines.push_back(std::move(line));
    }
};

void render_message(const loom::Value& value, std::int64_t depth, Sheet& sheet);

/// ONE CELL, AS THE TAIL OF A LINE THAT ALREADY CARRIES ITS LABEL -- or as a whole
/// block underneath it, for the two kinds that have interiors.
///
/// IT SWITCHES ON THE CELL'S OWN KIND AND NEVER ON THE FIELD'S DECLARED KIND, which
/// is what makes it total: `Value` does not validate at construction, the typed
/// accessors throw on a mismatch, and a presenter that trusted a declaration would
/// crash on exactly the value most worth looking at. The declaration is a claim; the
/// storage is the fact.
///
/// RETURNS the inline tail (empty when the cell wrote its own rows).
inline std::string render_cell(const loom::Cell& cell, const std::string& label,
                               std::int64_t depth, Sheet& sheet) {
    switch (cell.kind()) {
    case loom::Kind::Int:
        return std::to_string(cell.as_int());
    case loom::Kind::Float:
        return spell_float(cell.as_float());
    case loom::Kind::Text:
        return spell_text(cell.as_text());
    case loom::Kind::Bool:
        return cell.as_bool() ? "true" : "false";
    case loom::Kind::Bytes:
        // AN HONEST SUMMARY AND NOT THE OCTETS. Bytes are the escape hatch: this
        // presenter has no way to know whether they are an image, a key or a
        // serialized value, and rendering them as base64 would be a codec's answer
        // to a person's question. The length is the one thing that is always true.
        return "(bytes, " + std::to_string(cell.as_bytes().size()) + " octets)";
    case loom::Kind::Message: {
        const std::shared_ptr<loom::Value>& nested = cell.as_message();
        if (nested == nullptr) {
            return "(no value)"; // unreachable through `Cell::message`; written anyway
        }
        if (depth + 1 > kSampleMaxDepth) {
            return kSampleDeeper;
        }
        sheet.say(indent_of(depth) + label);
        render_message(*nested, depth + 1, sheet);
        return std::string();
    }
    case loom::Kind::List: {
        const loom::Cell::Array& members = cell.as_list();
        sheet.say(indent_of(depth) + label + "  " + items_said(members.size()));
        if (depth + 1 > kSampleMaxDepth) {
            if (!members.empty()) {
                sheet.say(indent_of(depth + 1) + kSampleDeeper);
            }
            return std::string();
        }
        const std::size_t shown =
            members.size() <= kSampleMaxListItems ? members.size() : kSampleMaxListItems;
        for (std::size_t i = 0; i < shown && !sheet.cut; ++i) {
            // A MEMBER HAS NO NAME, so its label is its position -- which is what a
            // reader needs in order to say "the third one" about a list they can
            // only see part of.
            const std::string member_label = "[" + std::to_string(i) + "]";
            const std::string tail = render_cell(members[i], member_label, depth + 1, sheet);
            if (!tail.empty()) {
                sheet.say(indent_of(depth + 1) + member_label + "  " + tail);
            }
        }
        if (shown < members.size()) {
            // COUNTED, because the total is known before a member is rendered.
            sheet.say(indent_of(depth + 1) + kSampleElided + " " +
                      std::to_string(members.size() - shown) + " more");
        }
        return std::string();
    }
    }
    // TOTAL OVER THE ENUMERATION, including a kind a later Loom appends. A named
    // fallback is a true sentence about a value this build cannot spell; a crash or
    // a blank is not.
    return std::string("(") + loom::name_of(cell.kind()) + ", not shown)";
}

/// EVERY FIELD OF ONE MESSAGE, in the schema's own order.
///
/// THE NAMES ARE PADDED TO THE WIDEST AT THIS LEVEL and nowhere else, because
/// alignment is a property of a column and a nested message is a different column.
/// `fit` on the pane side may still cut the row; that is the pane's budget and this
/// is the value's shape.
inline void render_message(const loom::Value& value, std::int64_t depth, Sheet& sheet) {
    const std::vector<loom::Field>& fields = value.schema().fields();
    std::size_t widest = 0;
    for (const loom::Field& f : fields) {
        widest = f.name.size() > widest ? f.name.size() : widest;
    }
    for (std::size_t i = 0; i < fields.size() && !sheet.cut; ++i) {
        const std::string label = fields[i].name + std::string(widest - fields[i].name.size(), ' ');
        const loom::Cell* cell = value.at(i);
        if (cell == nullptr) {
            sheet.say(indent_of(depth) + label + "  " + kSampleAbsent);
            continue;
        }
        const std::string tail = render_cell(*cell, label, depth, sheet);
        if (!tail.empty()) {
            sheet.say(indent_of(depth) + label + "  " + tail);
        }
    }
}

} // namespace detail

/// THE WHOLE OF WHAT A SAMPLE LOOKS LIKE, as logical lines.
///
/// THE FIRST LINE IS THE SCHEMA'S IDENTITY, and it is first because it is what the
/// rest of the lines are ABOUT: two Sources may both answer with a `source` field
/// and mean entirely different things, and the schema name and version are how a
/// maker tells which one they are reading.
///
/// PURE. It reads a value, allocates strings and returns them. No clock, no
/// counter, no static state, no logging, no I/O, and nothing about the identity
/// that produced the value -- so the same value renders identically wherever and
/// whenever it is asked, which is what makes it testable over a value rather than
/// over a running host.
///
/// TOTAL over everything the public Value/Schema machinery admits: an absent field,
/// an empty message, an empty list, a nesting deeper than the bound, a list longer
/// than the bound, a Bytes field, and a kind this build has no richer spelling for
/// all produce a line rather than an exception.
inline std::vector<std::string> render_value(const loom::Value& value) {
    detail::Sheet sheet;
    sheet.say(value.schema().name() + " v" + std::to_string(value.schema().version()));
    detail::render_message(value, 1, sheet);
    return std::move(sheet.lines);
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SAMPLE_PRESENTATION_HPP
