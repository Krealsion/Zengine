// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PERSIST_HPP
#define ZENGINE_WORKSHOP_PERSIST_HPP

// The file doors every durable artifact shares: one safe write, one bounded read, and one way a
// relative spelling is resolved.
// Workshop law: agents/workshop/document-file.md (+3 registers; agents/workshop.md routes)

#include "property.hpp" // `Written`

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace zengine::workshop::persist {

/// The name the retired object document was saved under: kept so a launch can say that a file
/// under it is left exactly as it is (`workshop.cpp`), and never read as anything else.
inline constexpr const char* kRetiredDocumentName = "workshop.json";

// ---- The file itself -------------------------------------------------------

/// ONE SPELLING OF A PATH, RESOLVED AGAINST THE PLACE IT WAS MEANT RELATIVE TO.
// WL-EDIT-06 -- agents/workshop/editor.md; WL-PROJ-02 -- agents/workshop/project.md
inline std::string resolved_against(const std::string& base, const std::string& spelling) {
    if (spelling.empty()) {
        return spelling;
    }
    std::filesystem::path p(spelling);
    if (p.is_absolute()) {
        return p.lexically_normal().generic_string();
    }
    if (base.empty()) {
        return spelling;
    }
    return (std::filesystem::path(base) / p).lexically_normal().generic_string();
}

/// What reading a file produced.
struct FileText {
    Written outcome;
    std::string text;
};

/// The whole file, or why not. Refusals are the ordinary ones a weaver meets:
/// the file is not there, it cannot be opened, it is too big to be what it
/// claims to be.
// WL-DOC-15 -- agents/workshop/document-file.md
inline FileText read_file(const std::string& path, std::uintmax_t most, const char* what) {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        return FileText{Written::no("cannot read " + path + ": " + ec.message()), {}};
    }
    if (size > most) {
        return FileText{Written::no("cannot read " + path + ": " + std::to_string(size) +
                                    " bytes is larger than " + what + " can be"),
                        {}};
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return FileText{Written::no("cannot read " + path), {}};
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) {
        return FileText{Written::no("cannot read " + path + ": the read failed part way"), {}};
    }
    return FileText{Written::ok(), std::move(text)};
}

/// Where the complete candidate is written before it becomes the document.
inline std::string pending_path(const std::string& path) { return path + ".saving"; }

/// Write text to a file WITHOUT putting the last good save at risk.
// WL-SESSION-13 -- agents/workshop/session.md; WL-DOC-15 -- agents/workshop/document-file.md
inline Written write_file(const std::string& path, const std::string& text) {
    const std::string pending = pending_path(path);
    {
        std::ofstream out(pending, std::ios::binary | std::ios::trunc);
        if (!out) {
            return Written::no("cannot write " + pending);
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.close();
        if (!out) {
            std::error_code drop;
            std::filesystem::remove(pending, drop);
            return Written::no("could not finish writing " + pending +
                               " -- " + path + " is unchanged");
        }
    }
    std::error_code ec;
    std::filesystem::rename(pending, path, ec);
    if (ec) {
        std::error_code drop;
        std::filesystem::remove(pending, drop);
        return Written::no("cannot replace " + path + ": " + ec.message() +
                           " -- it is unchanged");
    }
    return Written::ok();
}

/// The same safe write, into a directory that may not exist yet: the per-user roots are created on
/// first write (the session, the prefs, the legacy import). Project files never are: a missing
/// directory there is a weaver's typo, refused loudly.
// WL-SESSION-18 -- agents/workshop/session.md
inline Written write_file_making_room(const std::string& path, const std::string& text) {
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return Written::no("cannot create " + parent.string() + ": " + ec.message());
        }
    }
    return write_file(path, text);
}

// ---- JSON read as text ---------------------------------------------------------------------
// For the few reads that meet a file before any shape can: an older file's header, and the members
// a file of lines puts together. Nothing here judges a value; a shape's admission does that.

namespace json {

/// Past the JSON string starting at `at` (a `"`): the index after its closing quote, or npos.
inline std::size_t past_string(std::string_view text, std::size_t at) {
    for (std::size_t i = at + 1; i < text.size(); ++i) {
        if (text[i] == '\\') {
            ++i;
        } else if (text[i] == '"') {
            return i + 1;
        }
    }
    return std::string_view::npos;
}

/// Past the JSON value starting at `at`: the index after it, or npos.
inline std::size_t past_value(std::string_view text, std::size_t at) {
    if (at >= text.size()) {
        return std::string_view::npos;
    }
    if (text[at] == '"') {
        return past_string(text, at);
    }
    if (text[at] != '{' && text[at] != '[') {
        std::size_t i = at;
        while (i < text.size() && text[i] != ',' && text[i] != '}' && text[i] != ']') {
            ++i;
        }
        return i;
    }
    int depth = 0;
    for (std::size_t i = at; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '"') {
            i = past_string(text, i);
            if (i == std::string_view::npos) {
                return i;
            }
            --i;
        } else if (c == '{' || c == '[') {
            ++depth;
        } else if (c == '}' || c == ']') {
            if (--depth == 0) {
                return i + 1;
            }
        }
    }
    return std::string_view::npos;
}

inline std::size_t skip_space(std::string_view text, std::size_t at) {
    while (at < text.size() && (text[at] == ' ' || text[at] == '\n' || text[at] == '\r' ||
                                text[at] == '\t')) {
        ++at;
    }
    return at;
}

/// THE VALUE OF MEMBER `key` OF THE JSON OBJECT `object`, as its text, or nothing: read at the
/// object's own level, never inside a nested value.
inline std::optional<std::string_view> member(std::string_view object, std::string_view key) {
    std::size_t at = skip_space(object, 0);
    if (at >= object.size() || object[at] != '{') {
        return std::nullopt;
    }
    ++at;
    for (;;) {
        at = skip_space(object, at);
        if (at >= object.size() || object[at] == '}') {
            return std::nullopt;
        }
        if (object[at] != '"') {
            return std::nullopt;
        }
        const std::size_t name_end = past_string(object, at);
        if (name_end == std::string_view::npos) {
            return std::nullopt;
        }
        const std::string_view name = object.substr(at + 1, name_end - at - 2);
        at = skip_space(object, name_end);
        if (at >= object.size() || object[at] != ':') {
            return std::nullopt;
        }
        at = skip_space(object, at + 1);
        const std::size_t value_end = past_value(object, at);
        if (value_end == std::string_view::npos) {
            return std::nullopt;
        }
        if (name == key) {
            return object.substr(at, value_end - at);
        }
        at = skip_space(object, value_end);
        if (at < object.size() && object[at] == ',') {
            ++at;
        }
    }
}

/// A JSON number or a string of digits as an integer, or nothing.
inline std::optional<std::int64_t> integer(std::optional<std::string_view> value) {
    if (!value) {
        return std::nullopt;
    }
    std::string_view v = *value;
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
        v = v.substr(1, v.size() - 2);
    }
    std::int64_t out = 0;
    const auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
    if (ec != std::errc{} || end != v.data() + v.size()) {
        return std::nullopt;
    }
    return out;
}

/// A JSON string's text, unescaped only where it holds no escape, or nothing.
inline std::optional<std::string> word(std::optional<std::string_view> value) {
    if (!value || value->size() < 2 || value->front() != '"' || value->back() != '"' ||
        value->find('\\') != std::string_view::npos) {
        return std::nullopt;
    }
    return std::string(value->substr(1, value->size() - 2));
}


/// EVERY MEMBER OF THE JSON OBJECT `object`, its name and its value's text, in the order written;
/// nothing when it is not an object this reader can walk.
inline std::optional<std::vector<std::pair<std::string_view, std::string_view>>>
members(std::string_view object) {
    std::vector<std::pair<std::string_view, std::string_view>> out;
    std::size_t at = skip_space(object, 0);
    if (at >= object.size() || object[at] != '{') {
        return std::nullopt;
    }
    ++at;
    for (;;) {
        at = skip_space(object, at);
        if (at < object.size() && object[at] == '}') {
            return out;
        }
        if (at >= object.size() || object[at] != '"') {
            return std::nullopt;
        }
        const std::size_t name_end = past_string(object, at);
        if (name_end == std::string_view::npos) {
            return std::nullopt;
        }
        const std::string_view name = object.substr(at + 1, name_end - at - 2);
        at = skip_space(object, name_end);
        if (at >= object.size() || object[at] != ':') {
            return std::nullopt;
        }
        at = skip_space(object, at + 1);
        const std::size_t value_end = past_value(object, at);
        if (value_end == std::string_view::npos) {
            return std::nullopt;
        }
        out.emplace_back(name, object.substr(at, value_end - at));
        at = skip_space(object, value_end);
        if (at < object.size() && object[at] == ',') {
            ++at;
        }
    }
}

} // namespace json

} // namespace zengine::workshop::persist

#endif // ZENGINE_WORKSHOP_PERSIST_HPP
