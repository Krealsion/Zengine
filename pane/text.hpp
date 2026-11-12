// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_PANE_TEXT_HPP
#define ZENGINE_PANE_TEXT_HPP

// The text a pane shapes before it publishes: fitted to the columns it was granted, wrapped,
// padded, and kept to what a row shows. One copy for every pane package. Every width is columns,
// each character taking the columns the text core gives it (`component::character_columns`), so a
// wide character takes two, a combining mark none, and no cut falls inside a character. The
// host's own `detail::fit` is separate: a host sharing code with the panes it judges would judge
// its own output, so where the two disagree, the pane's rows change.

#include "component/text.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::pane::pane_text {

/// THE MARK A CUT LEAVES, and the one every pane in this tree already used.
inline constexpr const char* kElided = "...";

/// How far a wrapped continuation row is indented, when the room can hold an indent at all.
inline constexpr std::int64_t kWrapIndent = 2;

/// How many columns a text takes (`component::text_columns`), as a signed width.
inline std::int64_t columns_of(std::string_view text) {
    return static_cast<std::int64_t>(component::text_columns(text));
}

/// ONE ROW, FITTED TO A COLUMN BUDGET, WITH THE CUT MARKED. A width at or below zero is no
/// room at all and answers with nothing; a width at or below the mark's own length answers
/// with as much of the mark as fits, because a row that says only "..." still says that
/// something was cut. The cut falls between whole characters.
inline std::string fit(std::string text, std::int64_t width) {
    if (width <= 0) {
        return {};
    }
    const std::size_t room = static_cast<std::size_t>(width);
    if (component::text_columns(text) <= room) {
        return text; // it fits, so nothing about it changes -- not even its role
    }
    const std::size_t mark = std::string_view(kElided).size();
    if (room <= mark) {
        return std::string(kElided).substr(0, room);
    }
    text.resize(component::cut_at_columns(text, 0, room - mark));
    text += kElided;
    return text;
}

/// HOW MUCH OF A PATH NAMES ITS FILESYSTEM: a share's `//server/`, a drive's `C:/`, POSIX's `/`,
/// and nothing of a relative path. A path spelled with generic slashes.
inline std::size_t path_root(std::string_view path) {
    if (path.size() >= 2 && path[0] == '/' && path[1] == '/') {
        const std::size_t at = path.find('/', 2); // the server's name and the slash after it
        return at == std::string_view::npos ? path.size() : at + 1;
    }
    if (!path.empty() && path[0] == '/') {
        return 1;
    }
    if (path.size() >= 3 && path[1] == ':' && path[2] == '/') {
        return 3;
    }
    return 0;
}

/// A PATH FITTED TO A COLUMN BUDGET BY ITS MIDDLE: its root (`path_root`), the mark, and as much of
/// its end as fits -- the drive that says which filesystem, and the end that says which file. A
/// path that fits is itself; a budget with no room for the root, the mark and one column of the
/// end is `fit`'s, cut at the end.
inline std::string fit_path(const std::string& path, std::int64_t width) {
    if (width <= 0) {
        return {};
    }
    const std::size_t room = static_cast<std::size_t>(width);
    if (component::text_columns(path) <= room) {
        return path;
    }
    const std::size_t mark = std::string_view(kElided).size();
    const std::size_t root = path_root(path);
    const std::size_t root_cols = component::columns_between(path, 0, root);
    if (root_cols + mark + 1 > room) {
        return fit(path, width);
    }
    std::size_t tail = component::start_for_columns(path, path.size(), room - root_cols - mark);
    tail = tail < root ? root : tail;
    return path.substr(0, root) + kElided + path.substr(tail);
}

/// A TEXT LONGER THAN ITS ROW, SHOWN WHOLE BY THE POINTER RESTING ON IT: `width` columns of it,
/// from where the pointer's `column` puts them -- the row's first column shows the text's start,
/// its last the text's end, and every column between an offset proportional to it, never moving
/// back as the column moves right. Nothing is cut and no mark is drawn. A text that fits is itself.
inline std::string scrub(const std::string& text, std::int64_t width, std::int64_t column) {
    if (width <= 0) {
        return {};
    }
    const std::size_t room = static_cast<std::size_t>(width);
    const std::size_t total = component::text_columns(text);
    if (total <= room) {
        return text;
    }
    const std::uint64_t hidden = total - room;
    const std::int64_t last = width - 1;
    const std::int64_t at = column < 0 ? 0 : (column > last ? last : column);
    const std::uint64_t offset =
        last == 0 ? 0
                  : (static_cast<std::uint64_t>(at) * hidden * 2 + static_cast<std::uint64_t>(last)) /
                        (static_cast<std::uint64_t>(last) * 2);
    const std::size_t from = component::byte_at_column(text, 0, static_cast<std::size_t>(offset));
    return text.substr(from, component::cut_at_columns(text, from, room) - from);
}

/// ONE ROW, PADDED TO A WIDTH IN COLUMNS -- and truncated to it, unmarked, at whole characters,
/// because a caller that pads is composing a column it has already measured rather than cutting a
/// weaver's prose. A wide character the cut leaves out leaves its column blank.
inline std::string pad(std::string text, std::size_t width) {
    std::size_t cols = component::text_columns(text);
    if (cols > width) {
        text.resize(component::cut_at_columns(text, 0, width));
        cols = component::text_columns(text);
    }
    text.append(width - cols, ' ');
    return text;
}

/// A sentence across as many rows as it needs, broken at a space where there is one and at the
/// budget where there is not, continuations indented, every row at most `width` columns. A break
/// spends one space, never a run: the rest are a weaver's own bytes.
inline std::vector<std::string> wrap(const std::string& text, std::int64_t width) {
    std::vector<std::string> rows;
    if (width <= 0) {
        return rows;
    }
    const std::size_t room = static_cast<std::size_t>(width);
    const std::size_t indent =
        width > kWrapIndent + 1 ? static_cast<std::size_t>(kWrapIndent) : 0;
    std::size_t at = 0;
    while (true) {
        const std::string lead(rows.empty() ? 0 : indent, ' ');
        const std::size_t take = room - lead.size();
        const std::size_t fits = component::cut_at_columns(text, at, take);
        if (fits >= text.size()) {
            rows.push_back(lead + text.substr(at));
            return rows;
        }
        std::size_t cut = fits;
        bool broke = false;
        for (std::size_t i = fits; i > at; --i) {
            if (text[i] == ' ') {
                cut = i;
                broke = true;
                break;
            }
        }
        if (!broke && cut <= at) {
            cut = component::character_end(text, at); // wider than the row: shown alone
        }
        rows.push_back(lead + text.substr(at, cut - at));
        at = cut;
        if (broke) {
            ++at; // the space that broke the line is spent by the break
        }
        if (at >= text.size()) {
            return rows;
        }
    }
}

/// THE ROW A LIST SPENDS ON WHAT IT COULD NOT SHOW. Never a bare count: `which` names what was
/// left out, so a weaver reads a sentence rather than a number.
inline std::string omitted_text(std::size_t how_many, const char* which) {
    return "... " + std::to_string(how_many) + " " + which;
}

/// A label in front of an editable value, shortened so the value keeps `floor` columns: the label
/// gives way, since it is not the half a weaver reads characters off. Returns `full` whenever both
/// fit. Read by painting and by the press handler alike, so a caret is never aimed at a column
/// the row did not draw.
inline std::string fitted_label(const std::string& full, std::int64_t columns,
                                std::int64_t floor) {
    const std::int64_t full_cols = columns_of(full);
    if (columns <= 0 || full_cols + floor <= columns) {
        return full;
    }
    return fit(full, columns - floor);
}

/// WORDS SOMEBODY ELSE WROTE, AS A ROW SHOWS THEM, and how many characters that changed
/// (`changed`, when given, grows by that many): every character as written, a tab or any other
/// control character one space, and a terminal escape sequence left out whole -- a CSI one to its
/// final byte, any other the escape alone. A row is one line, and an escape is a terminal's
/// instruction, not text.
// WL-OUT-03 -- agents/workshop/build-output.md
inline std::string printable(const std::string& text, std::size_t* changed = nullptr) {
    std::string out;
    out.reserve(text.size());
    std::size_t count = 0;
    const std::size_t n = text.size();
    std::size_t i = 0;
    while (i < n) {
        const unsigned char b = static_cast<unsigned char>(text[i]);
        if (b == 0x1Bu) {
            ++count;
            ++i;
            if (i < n && text[i] == '[') {
                ++i;
                while (i < n) {
                    const unsigned char c = static_cast<unsigned char>(text[i]);
                    ++i;
                    if (c >= 0x40u && c <= 0x7Eu) {
                        break;
                    }
                }
            }
            continue;
        }
        const std::size_t end = component::character_end(text, i);
        const char32_t cp = component::code_point_at(text, i);
        if (b < 0x20u || b == 0x7Fu || (cp >= 0x80u && cp < 0xA0u)) {
            out += ' ';
            ++count;
        } else {
            out.append(text, i, end - i);
        }
        i = end;
    }
    if (changed != nullptr) {
        *changed += count;
    }
    return out;
}

/// ...and the same question the other way: is this text, whole, one a row shows as it is written?
/// `printable` repairs what a pane is about to say; this refuses what a weaver typed or pasted --
/// a control character, or bytes that spell no character -- because silently changing their bytes
/// is worse. Two doors, two postures, one rule.
inline bool admissible(const std::string& text) {
    for (std::size_t i = 0; i < text.size(); i = component::character_end(text, i)) {
        const char32_t cp = component::code_point_at(text, i);
        if (cp == component::kNotACharacter || cp < 0x20u || (cp >= 0x7Fu && cp < 0xA0u)) {
            return false;
        }
    }
    return true;
}

} // namespace zengine::pane::pane_text

#endif // ZENGINE_PANE_TEXT_HPP
