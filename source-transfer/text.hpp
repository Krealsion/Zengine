// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_SOURCE_TRANSFER_TEXT_HPP
#define ZENGINE_SOURCE_TRANSFER_TEXT_HPP

// TEXT INTO LINES, JUDGED BY THE EDITOR THAT WILL HOLD THEM: one splitting rule and two byte laws.
//
// A break in carried text is LF, and CRLF is read as one break too -- the one deliberate
// normalization at insertion, because the receiving document writes breaks in its own convention.
// The standard Editor's law is its source-byte law (WL-EDIT-07): tab and printable ASCII, anything
// else refused whole, naming the line and the byte. Neovim holds any valid UTF-8 except NUL, which
// Neovim's own functions spell as a line break (measured) and so cannot be carried faithfully.
// Nothing is replaced, dropped or clamped: text either fits the receiving law or is refused.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::source_transfer {

/// TEXT AS AN EDITOR'S LINES, or the refusal in words. `lines` holds at least one line when
/// `ok`; text ending in a break ends in an empty line, exactly as a document holds it.
struct Lines {
    bool ok = false;
    std::vector<std::string> lines;
    std::string refusal;
};

inline bool valid_utf8(std::string_view s) noexcept {
    std::size_t i = 0;
    while (i < s.size()) {
        const auto c = static_cast<unsigned char>(s[i]);
        std::size_t extra = 0;
        std::uint32_t cp = 0;
        std::uint32_t lo = 0;
        if (c < 0x80u) {
            ++i;
            continue;
        }
        if ((c & 0xE0u) == 0xC0u) {
            extra = 1; cp = c & 0x1Fu; lo = 0x80u;
        } else if ((c & 0xF0u) == 0xE0u) {
            extra = 2; cp = c & 0x0Fu; lo = 0x800u;
        } else if ((c & 0xF8u) == 0xF0u) {
            extra = 3; cp = c & 0x07u; lo = 0x10000u;
        } else {
            return false;
        }
        if (i + extra >= s.size()) {
            return false; // a truncated sequence
        }
        for (std::size_t k = 1; k <= extra; ++k) {
            const auto b = static_cast<unsigned char>(s[i + k]);
            if ((b & 0xC0u) != 0x80u) {
                return false;
            }
            cp = (cp << 6) | (b & 0x3Fu);
        }
        if (cp < lo || cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
            return false;
        }
        i += extra + 1;
    }
    return true;
}

inline std::string byte_words(unsigned char b) {
    const char* hex = "0123456789abcdef";
    std::string out = "0x";
    out.push_back(hex[b >> 4]);
    out.push_back(hex[b & 0xFu]);
    return out;
}

namespace detail {
/// THE ONE SPLITTING RULE: LF breaks a line, and a CR directly before it belongs to the break.
/// `bare_cr` is the first CR that is not part of a CRLF, or npos.
inline std::vector<std::string> split(std::string_view text, std::size_t& bare_cr) {
    std::vector<std::string> lines(1);
    bare_cr = std::string_view::npos;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\n') {
            lines.emplace_back();
            continue;
        }
        if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
            continue; // the CR of a CRLF: the LF that follows is the break
        }
        if (c == '\r' && bare_cr == std::string_view::npos) {
            bare_cr = i;
        }
        lines.back().push_back(c);
    }
    return lines;
}
} // namespace detail

/// THE STANDARD EDITOR'S LAW: tab and printable ASCII within a line; a bare CR, any other control
/// byte and any byte outside ASCII refuse the whole text.
inline Lines standard_lines(std::string_view text) {
    Lines out;
    std::size_t bare_cr = std::string_view::npos;
    std::vector<std::string> lines = detail::split(text, bare_cr);
    for (std::size_t r = 0; r < lines.size(); ++r) {
        for (std::size_t b = 0; b < lines[r].size(); ++b) {
            const auto c = static_cast<unsigned char>(lines[r][b]);
            if (c == '\t' || (c >= 0x20u && c < 0x7Fu)) {
                continue;
            }
            out.refusal = "line " + std::to_string(r + 1) + " holds " +
                          (c == '\r' ? std::string("a carriage return that ends no line")
                           : c >= 0x80u ? "a byte outside plain ASCII (" + byte_words(c) + ")"
                                        : "a control byte (" + byte_words(c) + ")") +
                          ", which the standard Editor cannot hold";
            return out;
        }
    }
    out.ok = true;
    out.lines = std::move(lines);
    return out;
}

/// NEOVIM'S LAW: any valid UTF-8 but NUL; a bare CR stays a character of its line.
inline Lines neovim_lines(std::string_view text) {
    Lines out;
    if (!valid_utf8(text)) {
        out.refusal = "the text is not valid UTF-8";
        return out;
    }
    std::size_t bare_cr = std::string_view::npos;
    std::vector<std::string> lines = detail::split(text, bare_cr);
    for (std::size_t r = 0; r < lines.size(); ++r) {
        if (lines[r].find('\0') != std::string::npos) {
            out.refusal = "line " + std::to_string(r + 1) +
                          " holds a NUL byte, which Neovim would read back as a line break";
            return out;
        }
    }
    out.ok = true;
    out.lines = std::move(lines);
    return out;
}

inline std::string join_lf(const std::vector<std::string>& lines) {
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            out += '\n';
        }
        out += lines[i];
    }
    return out;
}

/// "3 lines" / "1 line" / "12 characters" -- how a notice says what moved.
inline std::string amount_words(const std::vector<std::string>& lines) {
    if (lines.size() > 1) {
        return std::to_string(lines.size()) + " lines";
    }
    std::size_t n = 0; // code points: a UTF-8 continuation byte begins no character
    for (const char c : lines.empty() ? std::string() : lines.front()) {
        n += (static_cast<unsigned char>(c) & 0xC0u) != 0x80u ? 1 : 0;
    }
    return std::to_string(n) + (n == 1 ? " character" : " characters");
}

} // namespace zengine::source_transfer

#endif
