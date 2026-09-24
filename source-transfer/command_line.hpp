// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_SOURCE_TRANSFER_COMMAND_LINE_HPP
#define ZENGINE_SOURCE_TRANSFER_COMMAND_LINE_HPP

// A TYPED COMMAND AS THE WORKSHOP TERMINAL'S LINE -- the one established, documented text a maker
// types to send a message (docs/workshop/terminal.md: `send <address> <Shape> <version>` and then
// `field=value` arguments, read by Loom's own lexer). Editable text, never an execution: inserting
// it runs nothing, and running it later is a new send under the sender's current authority.
//
// WHAT IT SAYS AND WHAT IT REFUSES TO GUESS. Every PRESENT field, in declaration order, named; an
// absent field stays absent, so a preset's missing required field is missing from the line too and
// the Terminal's composer asks for it, rather than meeting a value nobody authored. Text is always
// quoted, so a text that looks like a number, a boolean or a `$reference` stays text. The address
// is written only when the value's own capture supplied one that still means something (an office
// or a publish); otherwise the line carries `<address>`, which the Terminal refuses as no address.
// A value the grammar cannot spell -- a quote or a control byte inside a text (the grammar has no
// escape), a non-finite number, bytes, a list or a nested message -- is refused, not approximated.
// And the line is proved: it is read back through Loom's lexer and every field must come back as
// exactly the cell it came from, or the line is refused.

#include <zen/schema.hpp>
#include <zen/terminal/input_lex.hpp>
#include <zen/value.hpp>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

namespace zengine::source_transfer {

inline constexpr const char* kAddressPlaceholder = "<address>";

struct TerminalLine {
    bool ok = false;
    std::string line;
    std::string refusal;
    std::vector<std::string> missing; ///< required fields the value does not hold
    bool address_supplied = false;    ///< false: the line carries `kAddressPlaceholder`
};

namespace detail {

/// A FLOAT AS A LITERAL LOOM'S LEXER READS AS A FLOAT: the shortest spelling that round-trips,
/// with a `.` it can see (`5` would read as an Int, `1e+20` as Text).
inline std::string float_literal(double v) {
    char buf[64];
    const auto r = std::to_chars(buf, buf + sizeof buf, v);
    std::string s(buf, r.ptr);
    if (s.find('.') == std::string::npos) {
        const std::size_t e = s.find_first_of("eE");
        s.insert(e == std::string::npos ? s.size() : e, ".0");
    }
    return s;
}

inline bool token_safe(std::string_view s) {
    if (s.empty()) {
        return false;
    }
    for (const char c : s) {
        const auto b = static_cast<unsigned char>(c);
        if (b <= 0x20u || b == 0x7Fu || c == '"') {
            return false;
        }
    }
    return true;
}

/// One field's argument, or why the grammar cannot say it.
inline std::optional<std::string> argument(const loom::Field& f, const loom::Cell& cell,
                                           std::string& why) {
    const std::string name = f.name + "=";
    switch (f.type.kind) {
    case loom::Kind::Bool:
        return name + (cell.as_bool() ? "true" : "false");
    case loom::Kind::Int:
        return name + std::to_string(cell.as_int());
    case loom::Kind::Float:
        if (!std::isfinite(cell.as_float())) {
            why = "field `" + f.name + "` holds a non-finite number, which a Terminal line cannot spell";
            return std::nullopt;
        }
        return name + float_literal(cell.as_float());
    case loom::Kind::Text: {
        for (const char c : cell.as_text()) {
            const auto b = static_cast<unsigned char>(c);
            if (c == '"') {
                why = "field `" + f.name + "` holds a double quote, which a Terminal line cannot escape";
                return std::nullopt;
            }
            if (b < 0x20u || b == 0x7Fu) {
                why = "field `" + f.name + "` holds a line break or control byte, which a Terminal line cannot hold";
                return std::nullopt;
            }
        }
        return name + "\"" + cell.as_text() + "\"";
    }
    case loom::Kind::Bytes:
        why = "field `" + f.name + "` holds bytes, which have no Terminal spelling";
        return std::nullopt;
    case loom::Kind::Message:
        why = "field `" + f.name + "` is a nested message, which a Terminal line cannot compose";
        return std::nullopt;
    case loom::Kind::List:
        why = "field `" + f.name + "` is a list, which a Terminal line cannot compose";
        return std::nullopt;
    }
    why = "field `" + f.name + "` has a kind this line does not know";
    return std::nullopt;
}

/// Does the lexer's reading of one argument equal the cell it was written from?
inline bool reads_back(const loom::Field& f, const loom::Cell& cell, const loom::Arg& arg) {
    if (!arg.name || *arg.name != f.name) {
        return false;
    }
    const auto* value = std::get_if<loom::FieldValue>(&arg.value);
    if (value == nullptr) {
        return false;
    }
    switch (f.type.kind) {
    case loom::Kind::Bool: {
        const bool* b = std::get_if<bool>(value);
        return b != nullptr && *b == cell.as_bool();
    }
    case loom::Kind::Int: {
        const std::int64_t* i = std::get_if<std::int64_t>(value);
        return i != nullptr && *i == cell.as_int();
    }
    case loom::Kind::Float: {
        const double* d = std::get_if<double>(value);
        return d != nullptr && (*d == cell.as_float()) && (std::signbit(*d) == std::signbit(cell.as_float()));
    }
    case loom::Kind::Text: {
        const std::string* s = std::get_if<std::string>(value);
        return s != nullptr && *s == cell.as_text();
    }
    default:
        return false;
    }
}

} // namespace detail

/// THE TERMINAL LINE FOR `value`, addressed to `address` when the capture supplied one (`@office`
/// or `*`) and to `kAddressPlaceholder` otherwise.
inline TerminalLine terminal_line(const loom::Value& value, std::string_view address) {
    TerminalLine out;
    const loom::Schema& schema = value.schema();
    if (!detail::token_safe(schema.name())) {
        out.refusal = "the shape's name `" + schema.name() + "` is not one word the Terminal can read";
        return out;
    }
    out.address_supplied = !address.empty();
    const std::string addr = address.empty() ? std::string(kAddressPlaceholder) : std::string(address);
    if (!detail::token_safe(addr)) {
        out.refusal = "the address `" + addr + "` is not one word the Terminal can read";
        return out;
    }
    std::string line = "send " + addr + " " + schema.name() + " " + std::to_string(schema.version());
    std::vector<std::pair<const loom::Field*, const loom::Cell*>> written;
    for (std::size_t i = 0; i < schema.fields().size(); ++i) {
        const loom::Field& f = schema.fields()[i];
        const loom::Cell* cell = value.at(i);
        if (cell == nullptr) {
            if (f.required) {
                out.missing.push_back(f.name);
            }
            continue;
        }
        std::string why;
        const std::optional<std::string> arg = detail::argument(f, *cell, why);
        if (!arg) {
            out.refusal = why;
            return out;
        }
        line += " " + *arg;
        written.emplace_back(&f, cell);
    }
    // THE PROOF: Loom's own lexer reads the line back, and every argument must be the cell it was.
    const std::vector<loom::Token> tokens = loom::tokenize(line);
    if (tokens.size() != 4 + written.size() || tokens[0].text != "send" || tokens[1].text != addr ||
        tokens[2].text != schema.name() || tokens[3].text != std::to_string(schema.version())) {
        out.refusal = "the Terminal would read this line's words differently";
        return out;
    }
    for (std::size_t i = 0; i < written.size(); ++i) {
        if (!detail::reads_back(*written[i].first, *written[i].second, loom::lex_arg(tokens[4 + i]))) {
            out.refusal = "the Terminal would read field `" + written[i].first->name + "` as a different value";
            return out;
        }
    }
    out.ok = true;
    out.line = std::move(line);
    return out;
}

} // namespace zengine::source_transfer

#endif
