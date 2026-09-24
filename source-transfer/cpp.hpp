// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_SOURCE_TRANSFER_CPP_HPP
#define ZENGINE_SOURCE_TRANSFER_CPP_HPP

// C++ THAT BUILDS A DROPPED COMMAND'S TYPED VALUE -- the optional, explicitly chosen conversion a
// maker asks for in a C++ document. It is written against Loom's generic typed-value API
// (`loom::SchemaBuilder`, `loom::Value`, `loom::Cell` from `<zen/schema.hpp>` and
// `<zen/value.hpp>`, target `loom::core`), because a runtime schema name is not a C++ type or an
// include path: the generated schema is the value's own, field by field, so the content-derived
// identity a receiver admits by is the same one the dropped value had.
//
// WHAT IT DOES NOT INVENT. No send, no destination, no `mail`, no build change, no include written
// into the document: the function returns the value and nothing else, and the includes it needs
// are said in its header comment -- present or missing in this document, by name. A required field
// the value does not hold (a preset's) is left as a labelled hole that does not compile, never a
// default. Flat values only: a list or nested-message field refuses generation, naming the field.
// Payload strings are escaped data (octal for every byte outside printable ASCII), never code.

#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::source_transfer {

/// WHETHER A DOCUMENT IS C++: by extension, and `.h` is honestly unknown -- C or C++ -- until a
/// language owner (Neovim's filetype) or the maker's explicit choice says which.
enum class CppDocument { No, Yes, Ambiguous };

inline CppDocument cpp_document(std::string_view path) {
    const std::size_t dot = path.find_last_of('.');
    const std::size_t slash = path.find_last_of("/\\");
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) {
        return CppDocument::No;
    }
    std::string ext(path.substr(dot + 1));
    for (char& c : ext) {
        c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
    for (const char* known : {"cpp", "cc", "cxx", "c++", "hpp", "hh", "hxx", "h++", "ipp", "tpp",
                              "inl", "ixx", "cppm"}) {
        if (ext == known) {
            return CppDocument::Yes;
        }
    }
    return ext == "h" ? CppDocument::Ambiguous : CppDocument::No;
}

struct GeneratedCpp {
    bool ok = false;
    std::vector<std::string> lines;           ///< the code, one entry per line
    std::string function;                     ///< the function it defines
    std::vector<std::string> needs;           ///< headers it needs
    std::vector<std::string> missing_includes; ///< of those, the ones the document lacks
    std::vector<std::string> holes;           ///< required fields left for the maker to fill
    std::string refusal;
};

namespace detail {

inline std::string cpp_string(std::string_view s) {
    std::string out = "\"";
    for (const char c : s) {
        const auto b = static_cast<unsigned char>(c);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (b >= 0x20u && b < 0x7Fu) {
            out += c;
        } else {
            out += '\\';
            out += static_cast<char>('0' + ((b >> 6) & 7u));
            out += static_cast<char>('0' + ((b >> 3) & 7u));
            out += static_cast<char>('0' + (b & 7u));
        }
    }
    out += '"';
    return out;
}

inline std::string cpp_double(double v) {
    char buf[64];
    const auto r = std::to_chars(buf, buf + sizeof buf, v);
    std::string s(buf, r.ptr);
    if (s.find_first_of(".eE") == std::string::npos) {
        s += ".0";
    }
    return s;
}

inline std::string cpp_int(std::int64_t v) {
    if (v == std::numeric_limits<std::int64_t>::min()) {
        return "-9223372036854775807 - 1"; // the one int64 value with no literal of its own
    }
    return std::to_string(v);
}

inline const char* kind_name(loom::Kind k) {
    switch (k) {
    case loom::Kind::Int: return "Int";
    case loom::Kind::Float: return "Float";
    case loom::Kind::Text: return "Text";
    case loom::Kind::Bool: return "Bool";
    case loom::Kind::Bytes: return "Bytes";
    case loom::Kind::Message: return "Message";
    case loom::Kind::List: return "List";
    }
    return "?";
}

/// `EnsureTimer` -> `ensure_timer`; the last dotted segment, lower snake case, identifier-safe.
inline std::string snake(std::string_view name) {
    const std::size_t dot = name.find_last_of('.');
    const std::string_view last = dot == std::string_view::npos ? name : name.substr(dot + 1);
    std::string out;
    for (std::size_t i = 0; i < last.size(); ++i) {
        const char c = last[i];
        if (c >= 'A' && c <= 'Z') {
            if (!out.empty() && out.back() != '_') {
                out += '_';
            }
            out += static_cast<char>(c - 'A' + 'a');
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out += c;
        } else if (!out.empty() && out.back() != '_') {
            out += '_';
        }
    }
    while (!out.empty() && out.back() == '_') {
        out.pop_back();
    }
    if (out.empty() || (out.front() >= '0' && out.front() <= '9')) {
        out = "value_" + out;
    }
    return out;
}

inline bool includes(const std::vector<std::string>& document, std::string_view header) {
    for (const std::string& line : document) {
        std::size_t i = line.find_first_not_of(" \t");
        if (i == std::string::npos || line[i] != '#') {
            continue;
        }
        i = line.find_first_not_of(" \t", i + 1);
        if (i == std::string::npos || line.compare(i, 7, "include") != 0) {
            continue;
        }
        if (line.find(header, i + 7) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace detail

/// THE FUNCTION THAT BUILDS `value`, for a document whose lines are `document` (read only, to say
/// which of the needed includes it already has).
inline GeneratedCpp cpp_value_function(const loom::Value& value, const std::vector<std::string>& document) {
    GeneratedCpp out;
    const loom::Schema& schema = value.schema();
    for (const loom::Field& f : schema.fields()) {
        if (f.type.kind == loom::Kind::Message || f.type.kind == loom::Kind::List) {
            out.refusal = "field `" + f.name + "` is a " +
                          (f.type.kind == loom::Kind::List ? std::string("list") : std::string("nested message")) +
                          "; this generator writes flat values only";
            return out;
        }
    }
    const std::size_t at = value.schema().fields().size();
    for (std::size_t i = 0; i < at; ++i) {
        const loom::Cell* cell = value.at(i);
        if (cell != nullptr && cell->kind() == loom::Kind::Float && !std::isfinite(cell->as_float())) {
            out.refusal = "field `" + schema.fields()[i].name + "` holds a non-finite number";
            return out;
        }
    }
    out.function = "make_" + detail::snake(schema.name()) + "_v" + std::to_string(schema.version());
    out.needs = {"<zen/schema.hpp>", "<zen/value.hpp>"};
    const bool umbrella = detail::includes(document, "<zen/zen.hpp>");
    for (const std::string& h : out.needs) {
        if (!umbrella && !detail::includes(document, h)) {
            out.missing_includes.push_back(h);
        }
    }
    for (std::size_t i = 0; i < at; ++i) {
        if (value.at(i) == nullptr && schema.fields()[i].required) {
            out.holes.push_back(schema.fields()[i].name);
        }
    }
    const std::string title = schema.name() + " v" + std::to_string(schema.version());
    std::vector<std::string>& l = out.lines;
    l.push_back("// ---- Zengine: C++ that builds " + title + ", generated from a dropped value ----");
    l.push_back("// It builds the typed value only: it sends nothing. Nothing was saved or built.");
    l.push_back("// Needs #include <zen/schema.hpp> and #include <zen/value.hpp> (Loom's loom::core).");
    if (out.missing_includes.empty()) {
        l.push_back("// This document already includes them.");
    } else {
        std::string lacks = "// This document lacks ";
        for (std::size_t i = 0; i < out.missing_includes.size(); ++i) {
            lacks += (i > 0 ? " and " : "") + out.missing_includes[i];
        }
        lacks += out.missing_includes.size() == 1 ? ": add it" : ": add them";
        l.push_back(lacks + " where your includes live.");
    }
    l.push_back("// The schema below must stay exactly " + title + "'s, or a receiver refuses the value.");
    if (!out.holes.empty()) {
        std::string holes = "// INCOMPLETE -- fill the required field";
        holes += out.holes.size() == 1 ? " " : "s ";
        for (std::size_t i = 0; i < out.holes.size(); ++i) {
            holes += (i > 0 ? ", " : "") + out.holes[i];
        }
        holes += " before this compiles.";
        l.push_back(holes);
    }
    l.push_back("inline loom::Value " + out.function + "() {");
    l.push_back("    static const std::shared_ptr<const loom::Schema> schema =");
    l.push_back("        loom::SchemaBuilder(" + detail::cpp_string(schema.name()) + ", " +
                std::to_string(schema.version()) + ")");
    for (const loom::Field& f : schema.fields()) {
        l.push_back(std::string("            .field(") + detail::cpp_string(f.name) + ", loom::Kind::" +
                    detail::kind_name(f.type.kind) + (f.required ? ")" : ", false)"));
    }
    l.push_back("            .build();");
    l.push_back("    loom::Value value(schema);");
    for (std::size_t i = 0; i < at; ++i) {
        const loom::Field& f = schema.fields()[i];
        const loom::Cell* cell = value.at(i);
        const std::string set = "    value.set(" + detail::cpp_string(f.name) + ", ";
        if (cell == nullptr) {
            if (f.required) {
                l.push_back(set + "/* FILL: required " + detail::kind_name(f.type.kind) + " */);");
            }
            continue;
        }
        switch (f.type.kind) {
        case loom::Kind::Int: l.push_back(set + "loom::Cell::integer(" + detail::cpp_int(cell->as_int()) + "));"); break;
        case loom::Kind::Float: l.push_back(set + "loom::Cell::real(" + detail::cpp_double(cell->as_float()) + "));"); break;
        case loom::Kind::Text: l.push_back(set + "loom::Cell::text(" + detail::cpp_string(cell->as_text()) + "));"); break;
        case loom::Kind::Bool: l.push_back(set + "loom::Cell::boolean(" + (cell->as_bool() ? "true" : "false") + "));"); break;
        case loom::Kind::Bytes: {
            std::string bytes = "loom::Cell::bytes(loom::Bytes{";
            const loom::Bytes& b = cell->as_bytes();
            for (std::size_t k = 0; k < b.size(); ++k) {
                bytes += (k > 0 ? ", " : "") + std::to_string(static_cast<unsigned>(b[k]));
            }
            l.push_back(set + bytes + "}));");
            break;
        }
        default: break;
        }
    }
    l.push_back("    return value;");
    l.push_back("}");
    l.push_back("// ---- end of generated C++ ----");
    out.ok = true;
    return out;
}

} // namespace zengine::source_transfer

#endif
