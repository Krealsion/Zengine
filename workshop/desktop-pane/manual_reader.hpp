// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESKTOP_PANE_MANUAL_READER_HPP
#define ZENGINE_DESKTOP_PANE_MANUAL_READER_HPP

// What a pane's `?` shows: the manual that pane answered, laid out for a weaver -- `About`, `Use`,
// then each command with the key it answers to now. The words are the page's and the keys the
// host's: where a key marker stands is the build's data (`PaneDocumentKey`), never found by
// reading the words. Pure, so the Manual pane composes from it and a case reads it alone.
// Workshop law: agents/workshop/desktop-presenting.md

#include "component/text_area.hpp"
#include "graphics/role.hpp"
#include "manual/manual.hpp"
#include "graphics/vocabulary.hpp"
#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/keymap.hpp"
#include "workshop/pane_document.hpp"
#include "workshop/pane_text.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::desktop_pane::manual_reader {

namespace ws = zengine::workshop;

/// THE KEYS ONE COMMAND ANSWERS TO NOW, as the Hotkeys view spells them: every key a row in force
/// gives it, else every key the weaver's file names for it, else the default the page wrote,
/// respelled; `unbound` where that leaves none.
// WL-DESK-17 -- agents/workshop/desktop-presenting.md
inline std::string keys_of(const ws::KeymapShown& keymap, const std::string& id,
                           const std::string& written) {
    std::vector<std::string> keys;
    bool found = false;
    const auto take = [&](const std::vector<ws::ShownBinding>& rows) {
        for (const ws::ShownBinding& b : rows) {
            if (b.id != id) {
                continue;
            }
            found = true;
            if (!b.shown.empty() && std::find(keys.begin(), keys.end(), b.shown) == keys.end()) {
                keys.push_back(b.shown);
            }
        }
    };
    take(keymap.rows);
    if (!found) {
        take(keymap.moved);
    }
    if (!found) {
        const ws::ParsedGesture parsed = ws::parse_gesture(written);
        if (parsed.accepted && ws::is_bound(parsed.gesture)) {
            keys.push_back(ws::spell_gesture(parsed.gesture, ws::spelling_of(keymap.spelling)));
        }
    }
    if (keys.empty()) {
        return "unbound";
    }
    std::string out = keys.front();
    for (std::size_t i = 1; i < keys.size(); ++i) {
        out += ", " + keys[i];
    }
    return out;
}

/// A SECTION'S WORDS WITH EVERY KEY MARKER SHOWING THE KEY THE WEAVER HAS NOW, in backticks as the
/// page writes a key. A marker the words do not hold where it says is left as written.
// WL-DESK-17 -- agents/workshop/desktop-presenting.md
inline std::string with_keys(const ws::PaneDocumentSection& section,
                             const ws::KeymapShown& keymap) {
    const std::string& text = section.text;
    std::string out;
    std::size_t from = 0;
    for (const ws::PaneDocumentKey& mark : section.keys) {
        if (mark.at < 0 || mark.length <= 0) {
            continue;
        }
        const std::size_t at = static_cast<std::size_t>(mark.at);
        const std::size_t length = static_cast<std::size_t>(mark.length);
        if (at < from || at > text.size() || length > text.size() - at) {
            continue;
        }
        out.append(text, from, at - from);
        out += "`" + keys_of(keymap, mark.id, mark.written) + "`";
        from = at + length;
    }
    out.append(text, from, std::string::npos);
    return out;
}

/// INLINE MARKDOWN MADE PLAIN FOR A READER: a link reads as its words and bold as its words;
/// code spans stay as written, backticks and all.
inline std::string plain(const std::string& text) {
    std::string out;
    const std::size_t n = text.size();
    // The index of `close` at or after `from`, stepping over code spans; npos when none.
    const auto closing = [&text, n](std::size_t from, char close) {
        for (std::size_t i = from; i < n; ++i) {
            if (text[i] == '`') {
                const std::size_t end = text.find('`', i + 1);
                if (end == std::string::npos) {
                    return std::string::npos;
                }
                i = end;
            } else if (text[i] == close) {
                return i;
            }
        }
        return std::string::npos;
    };
    std::size_t i = 0;
    while (i < n) {
        const char c = text[i];
        if (c == '`') {
            const std::size_t end = text.find('`', i + 1);
            const std::size_t stop = end == std::string::npos ? n : end + 1;
            out.append(text, i, stop - i);
            i = stop;
            continue;
        }
        if (c == '*' && i + 1 < n && text[i + 1] == '*') {
            i += 2;
            continue;
        }
        // ...and emphasis, a `*` against a word on one side and not the other.
        if (c == '*') {
            const auto word = [](char x) {
                return (x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || (x >= '0' && x <= '9') ||
                       x == '`';
            };
            const bool before = i > 0 && word(text[i - 1]);
            const bool after = i + 1 < n && word(text[i + 1]);
            if (before != after) {
                ++i;
                continue;
            }
        }
        if (c == '[') {
            const std::size_t label_end = closing(i + 1, ']');
            if (label_end != std::string::npos && label_end + 1 < n && text[label_end + 1] == '(') {
                const std::size_t target_end = text.find(')', label_end + 2);
                if (target_end != std::string::npos) {
                    out += plain(text.substr(i + 1, label_end - i - 1));
                    i = target_end + 1;
                    continue;
                }
            }
        }
        out += c;
        ++i;
    }
    return out;
}

/// A SECTION'S WORDS AS PARAGRAPHS: the lines of one paragraph joined, a list item or a table row
/// starting its own, a code block's lines kept as they are.
inline std::vector<std::string> paragraphs(const std::string& text) {
    std::vector<std::string> out;
    std::string open;
    bool fence = false;
    const auto close = [&out, &open] {
        if (!open.empty()) {
            out.push_back(open);
            open.clear();
        }
    };
    std::size_t from = 0;
    while (from <= text.size()) {
        const std::size_t nl = text.find('\n', from);
        const std::string line =
            text.substr(from, nl == std::string::npos ? std::string::npos : nl - from);
        from = nl == std::string::npos ? text.size() + 1 : nl + 1;
        if (line.rfind("```", 0) == 0) {
            close();
            fence = !fence;
            continue;
        }
        if (fence) {
            out.push_back(line);
            continue;
        }
        if (line.find_first_not_of(' ') == std::string::npos) { // blank, or spaces alone
            close();
            continue;
        }
        const bool starts = line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0 ||
                            line.rfind("|", 0) == 0;
        if (starts) {
            close();
        }
        // A continuation line's own indent is the source's layout, not the words'.
        const std::size_t words = line.find_first_not_of(' ');
        open = open.empty() ? line : open + " " + line.substr(words);
        if (line.rfind("|", 0) == 0) {
            close();
        }
    }
    close();
    return out;
}

/// A GROUP HEADING'S KEY, as the page's splitter makes one (`cmake/ZengineManual.cmake`): the key
/// of the words that open the group, which name no command.
inline std::string group_key(const std::string& heading) {
    std::string out;
    for (const char c : heading) {
        const char lower = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        if (lower == ' ') {
            out += '-';
        } else if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9') ||
                   lower == '-') {
            out += lower;
        }
    }
    return out;
}

/// THE MANUAL AS A TEXT AREA HOLDS IT: one paragraph a line of the text as written -- `About` and
/// `Use`, then each group of commands under its heading, each command's own heading carrying the
/// keys it answers to now -- each with its look: a heading in the accent role, a list item's further
/// lines under its words. The area wraps it to whatever width shows it.
struct ManualText {
    std::string text;
    std::vector<component::TextLook> looks;
};

// WL-DESK-17 -- agents/workshop/desktop-presenting.md
inline ManualText manual_text(const ws::PaneDocumentShown& shown, const ws::KeymapShown& keymap) {
    ManualText out;
    const auto line = [&out](const std::string& text, std::int64_t role, std::int64_t hang = 0) {
        if (!out.looks.empty()) {
            out.text += '\n';
        }
        out.text += ws::pane_text::drawable(text);
        out.looks.push_back(component::TextLook{role, hang});
    };
    const auto words = [&](const std::string& text, std::int64_t role) {
        // A blank line parts two paragraphs, and never two items of one list.
        bool first = true, last_item = false;
        for (const std::string& paragraph : paragraphs(text)) {
            const bool item = paragraph.rfind("- ", 0) == 0 || paragraph.rfind("* ", 0) == 0;
            if (!first && !(item && last_item)) {
                line(std::string(), graphics::role::kFill);
            }
            first = false;
            last_item = item;
            line(plain(paragraph), role, item ? ws::pane_text::kWrapIndent : 0);
        }
    };
    std::string group;
    for (const ws::PaneDocumentSection& s : shown.sections) {
        const bool opening = s.key == "about" || s.key == "use";
        const bool command = manual::is_command_group(s.group);
        if (!opening && !command) {
            continue;
        }
        if (!out.looks.empty()) {
            line(std::string(), graphics::role::kFill);
        }
        if (s.group != group) {
            group = s.group;
            line(group, graphics::role::kAccent);
        }
        if (command && s.key != group_key(s.group)) {
            // The default a command's own marker writes is its heading's too, for a command that is
            // not in force now and that the weaver's file does not move.
            std::string written;
            for (const ws::PaneDocumentKey& mark : s.keys) {
                if (mark.id == s.key) {
                    written = mark.written;
                    break;
                }
            }
            line(s.key + " -- " + keys_of(keymap, s.key, written), graphics::role::kAccent);
        }
        words(with_keys(s, keymap), graphics::role::kFill);
    }
    return out;
}

} // namespace zengine::desktop_pane::manual_reader

#endif // ZENGINE_DESKTOP_PANE_MANUAL_READER_HPP
