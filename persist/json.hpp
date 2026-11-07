// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_PERSIST_JSON_HPP
#define ZENGINE_PERSIST_JSON_HPP

// A PLAIN JSON DOCUMENT, read whole into a tree and written back: objects (their members in the
// order written, a name never twice), lists, strings, whole numbers and flags. For a file a weaver
// writes by hand, which no shape's envelope wraps; what the tree means is its reader's to judge.
// `null`, a fraction and an exponent are refused by name, as is anything nested past
// `kMaxJsonDepth` or a document holding more than `kMaxJsonNodes` values.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::persist::json {

/// How deep a value may nest, and how many values one document may hold.
inline constexpr std::size_t kMaxJsonDepth = 16;
inline constexpr std::size_t kMaxJsonNodes = 1u << 16;

/// ONE VALUE OF A DOCUMENT: its kind, and the part of it that kind holds.
struct Node {
    enum class Kind { kObject, kList, kText, kNumber, kFlag };
    Kind kind = Kind::kObject;
    std::vector<std::pair<std::string, Node>> members; ///< an object's, in the order written
    std::vector<Node> items;                           ///< a list's
    std::string text;
    std::int64_t number = 0;
    bool flag = false;

    /// An object's member named `name`, or nothing.
    const Node* member(std::string_view name) const {
        for (const auto& [n, v] : members) {
            if (n == name) {
                return &v;
            }
        }
        return nullptr;
    }
};

/// The word a sentence names a kind by.
inline const char* kind_word(Node::Kind k) {
    switch (k) {
    case Node::Kind::kObject: return "an object";
    case Node::Kind::kList: return "a list";
    case Node::Kind::kText: return "a text";
    case Node::Kind::kNumber: return "a number";
    case Node::Kind::kFlag: return "a flag";
    }
    return "a value";
}

/// What reading a document came to: its root, or why not, with the line it stopped on.
struct Parsed {
    std::optional<Node> root;
    std::string refusal;
};

namespace detail {

struct Reader {
    std::string_view text;
    std::size_t at = 0;
    std::size_t nodes = 0;
    std::string refusal;

    std::size_t line() const {
        std::size_t n = 1;
        for (std::size_t i = 0; i < at && i < text.size(); ++i) {
            n += text[i] == '\n' ? 1u : 0u;
        }
        return n;
    }

    bool fail(std::string why) {
        if (refusal.empty()) {
            refusal = "line " + std::to_string(line()) + ": " + std::move(why);
        }
        return false;
    }

    void space() {
        while (at < text.size() &&
               (text[at] == ' ' || text[at] == '\t' || text[at] == '\n' || text[at] == '\r')) {
            ++at;
        }
    }

    bool word(std::string_view w) {
        if (text.substr(at, w.size()) != w) {
            return false;
        }
        at += w.size();
        return true;
    }

    static void put_utf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool hex4(std::uint32_t& out) {
        if (at + 4 > text.size()) {
            return fail("a \\u escape needs four hex digits");
        }
        out = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            const char c = text[at + i];
            out <<= 4;
            if (c >= '0' && c <= '9') {
                out |= static_cast<std::uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                out |= static_cast<std::uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                out |= static_cast<std::uint32_t>(c - 'A' + 10);
            } else {
                return fail("a \\u escape needs four hex digits");
            }
        }
        at += 4;
        return true;
    }

    bool string(std::string& out) {
        ++at; // the opening quote
        while (at < text.size()) {
            const char c = text[at];
            if (c == '"') {
                ++at;
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return fail("a text holds a control character; write it as an escape");
            }
            if (c != '\\') {
                out += c;
                ++at;
                continue;
            }
            if (++at >= text.size()) {
                break;
            }
            const char e = text[at++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                std::uint32_t cp = 0;
                if (!hex4(cp)) {
                    return false;
                }
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    std::uint32_t low = 0;
                    if (!word("\\u") || !hex4(low) || low < 0xDC00 || low > 0xDFFF) {
                        return fail("a \\u escape opens a pair it does not close");
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return fail("a \\u escape closes a pair it did not open");
                }
                put_utf8(out, cp);
                break;
            }
            default: return fail(std::string("`\\") + e + "` is no escape");
            }
        }
        return fail("a text is not closed");
    }

    bool number(Node& n) {
        const std::size_t from = at;
        const bool negative = text[at] == '-';
        if (negative) {
            ++at;
        }
        if (at >= text.size() || text[at] < '0' || text[at] > '9') {
            return fail("a number needs a digit");
        }
        if (text[at] == '0' && at + 1 < text.size() && text[at + 1] >= '0' && text[at + 1] <= '9') {
            return fail("a number does not start with 0");
        }
        std::uint64_t v = 0;
        const std::uint64_t most =
            negative ? static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1
                     : static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
            const auto d = static_cast<std::uint64_t>(text[at] - '0');
            if (v > (most - d) / 10) {
                return fail("`" + std::string(text.substr(from, at - from + 1)) +
                            "...` is past the largest whole number");
            }
            v = v * 10 + d;
            ++at;
        }
        if (at < text.size() && (text[at] == '.' || text[at] == 'e' || text[at] == 'E')) {
            return fail("a number here is whole: no fraction and no exponent");
        }
        n.kind = Node::Kind::kNumber;
        n.number = negative ? static_cast<std::int64_t>(0 - v) : static_cast<std::int64_t>(v);
        return true;
    }

    bool value(Node& n, std::size_t depth) {
        if (++nodes > kMaxJsonNodes) {
            return fail("the document holds more than " + std::to_string(kMaxJsonNodes) +
                        " values");
        }
        if (depth > kMaxJsonDepth) {
            return fail("a value nests more than " + std::to_string(kMaxJsonDepth) + " deep");
        }
        space();
        if (at >= text.size()) {
            return fail("the document ends where a value was due");
        }
        const char c = text[at];
        if (c == '{') {
            n.kind = Node::Kind::kObject;
            ++at;
            space();
            if (at < text.size() && text[at] == '}') {
                ++at;
                return true;
            }
            for (;;) {
                space();
                if (at >= text.size() || text[at] != '"') {
                    return fail("an object's member is named by a text");
                }
                std::string name;
                if (!string(name)) {
                    return false;
                }
                if (n.member(name) != nullptr) {
                    return fail("the member `" + name + "` is written twice");
                }
                space();
                if (at >= text.size() || text[at] != ':') {
                    return fail("the member `" + name + "` needs a `:`");
                }
                ++at;
                Node v;
                if (!value(v, depth + 1)) {
                    return false;
                }
                n.members.emplace_back(std::move(name), std::move(v));
                space();
                if (at < text.size() && text[at] == ',') {
                    ++at;
                    continue;
                }
                if (at < text.size() && text[at] == '}') {
                    ++at;
                    return true;
                }
                return fail("an object goes on with `,` or ends with `}`");
            }
        }
        if (c == '[') {
            n.kind = Node::Kind::kList;
            ++at;
            space();
            if (at < text.size() && text[at] == ']') {
                ++at;
                return true;
            }
            for (;;) {
                Node v;
                if (!value(v, depth + 1)) {
                    return false;
                }
                n.items.push_back(std::move(v));
                space();
                if (at < text.size() && text[at] == ',') {
                    ++at;
                    continue;
                }
                if (at < text.size() && text[at] == ']') {
                    ++at;
                    return true;
                }
                return fail("a list goes on with `,` or ends with `]`");
            }
        }
        if (c == '"') {
            n.kind = Node::Kind::kText;
            return string(n.text);
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return number(n);
        }
        if (word("true")) {
            n.kind = Node::Kind::kFlag;
            n.flag = true;
            return true;
        }
        if (word("false")) {
            n.kind = Node::Kind::kFlag;
            n.flag = false;
            return true;
        }
        if (word("null")) {
            return fail("`null` is no value here: leave the member out");
        }
        return fail(std::string("`") + c + "` starts no value");
    }
};

inline void write_text(std::string& out, std::string_view s) {
    out += '"';
    for (const char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                static const char* hex = "0123456789abcdef";
                out += "\\u00";
                out += hex[(static_cast<unsigned char>(c) >> 4) & 0xF];
                out += hex[static_cast<unsigned char>(c) & 0xF];
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

/// Whether `n` is written on one line: no object or list inside it.
inline bool flat(const Node& n) {
    for (const auto& m : n.members) {
        if (m.second.kind == Node::Kind::kObject || m.second.kind == Node::Kind::kList) {
            return false;
        }
    }
    for (const Node& i : n.items) {
        if (i.kind == Node::Kind::kObject || i.kind == Node::Kind::kList) {
            return false;
        }
    }
    return true;
}

inline void write(std::string& out, const Node& n, std::size_t indent) {
    const std::string pad(indent, ' ');
    const std::string inner(indent + 2, ' ');
    switch (n.kind) {
    case Node::Kind::kText: write_text(out, n.text); return;
    case Node::Kind::kNumber: out += std::to_string(n.number); return;
    case Node::Kind::kFlag: out += n.flag ? "true" : "false"; return;
    case Node::Kind::kObject:
        if (n.members.empty()) {
            out += "{}";
            return;
        }
        if (flat(n) && indent > 0) {
            out += "{ ";
            for (std::size_t i = 0; i < n.members.size(); ++i) {
                out += i == 0 ? "" : ", ";
                write_text(out, n.members[i].first);
                out += ": ";
                write(out, n.members[i].second, indent);
            }
            out += " }";
            return;
        }
        out += "{\n";
        for (std::size_t i = 0; i < n.members.size(); ++i) {
            out += inner;
            write_text(out, n.members[i].first);
            out += ": ";
            write(out, n.members[i].second, indent + 2);
            out += i + 1 < n.members.size() ? ",\n" : "\n";
        }
        out += pad + "}";
        return;
    case Node::Kind::kList:
        if (n.items.empty()) {
            out += "[]";
            return;
        }
        if (flat(n)) {
            out += "[";
            for (std::size_t i = 0; i < n.items.size(); ++i) {
                out += i == 0 ? "" : ", ";
                write(out, n.items[i], indent);
            }
            out += "]";
            return;
        }
        out += "[\n";
        for (std::size_t i = 0; i < n.items.size(); ++i) {
            out += inner;
            write(out, n.items[i], indent + 2);
            out += i + 1 < n.items.size() ? ",\n" : "\n";
        }
        out += pad + "]";
        return;
    }
}

} // namespace detail

/// READ `text` AS ONE PLAIN JSON DOCUMENT, whole: its root, or the first thing wrong with it and
/// the line it stands on. Total: nothing here throws, and nothing past the document is ignored.
inline Parsed parse(std::string_view text) {
    detail::Reader r{text, 0, 0, {}};
    Node root;
    if (!r.value(root, 0)) {
        return Parsed{std::nullopt, std::move(r.refusal)};
    }
    r.space();
    if (r.at != text.size()) {
        r.fail("something follows the document's end");
        return Parsed{std::nullopt, std::move(r.refusal)};
    }
    return Parsed{std::move(root), {}};
}

/// `n` AS TEXT: an object's members in the order it holds them, two spaces a level, and an object
/// or a list holding no other written on one line; a newline at the end.
inline std::string to_text(const Node& n) {
    std::string out;
    detail::write(out, n, 0);
    out += '\n';
    return out;
}

// ---- Building a node ------------------------------------------------------------------------

inline Node text_node(std::string s) {
    Node n;
    n.kind = Node::Kind::kText;
    n.text = std::move(s);
    return n;
}

inline Node number_node(std::int64_t v) {
    Node n;
    n.kind = Node::Kind::kNumber;
    n.number = v;
    return n;
}

inline Node flag_node(bool v) {
    Node n;
    n.kind = Node::Kind::kFlag;
    n.flag = v;
    return n;
}

inline Node list_node(std::vector<Node> items = {}) {
    Node n;
    n.kind = Node::Kind::kList;
    n.items = std::move(items);
    return n;
}

} // namespace zengine::persist::json

#endif // ZENGINE_PERSIST_JSON_HPP
