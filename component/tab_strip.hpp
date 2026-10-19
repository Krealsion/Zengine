// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPONENT_TAB_STRIP_HPP
#define ZENGINE_COMPONENT_TAB_STRIP_HPP

// A one-row strip of tabs over a settings list: which tabs a list of settings derives, and what of
// them a width shows. The tabs are the strip's own three words and the settings' tags between
// them -- pinned, all, each tag in order, kept -- derived from the rows each time, never stored. A
// width too narrow for them all shows a window grown outward from the active tab, and counts what
// it leaves out on each side, `<N` and `N>`; the window is derived from the active tab too, so no
// offset can go stale when the tags change. The active tab reads `*word*` and the others `[word]`,
// so which is active is never said by colour alone. Both functions are pure.
// Reference: component/docs/component.md.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::component {

/// The strip's own tabs, which no tag may be: the pinned settings, every setting, and the kept
/// rows no declaration takes.
inline constexpr std::string_view kPinnedTab = "pinned";
inline constexpr std::string_view kAllTab = "all";
inline constexpr std::string_view kKeptTab = "kept";

/// THE TABS A SETTINGS LIST DERIVES, in order: `pinned` when any setting is pinned, `all`, every
/// tag the settings name in byte order and once each, and `kept` when a kept row is listed.
inline std::vector<std::string> setting_tabs(bool any_pinned, std::vector<std::string> tags,
                                             bool any_kept) {
    std::erase_if(tags, [](const std::string& t) {
        return t.empty() || t == kPinnedTab || t == kAllTab || t == kKeptTab;
    });
    std::ranges::sort(tags);
    const auto repeated = std::ranges::unique(tags);
    tags.erase(repeated.begin(), repeated.end());
    std::vector<std::string> out;
    out.reserve(tags.size() + 3);
    if (any_pinned) {
        out.emplace_back(kPinnedTab);
    }
    out.emplace_back(kAllTab);
    for (std::string& t : tags) {
        out.push_back(std::move(t));
    }
    if (any_kept) {
        out.emplace_back(kKeptTab);
    }
    return out;
}

/// WHERE ONE TAB IS DRAWN: its place in the tab list, its word, and the columns of its face,
/// brackets included -- the whole face is the target.
struct PlacedTab {
    std::size_t index = 0;
    std::string word;
    bool active = false;
    std::int64_t first = 0;
    std::int64_t width = 0;
};

/// THE STRIP AS A WIDTH SHOWS IT: the row to draw, the tabs placed on it, and on each side how
/// many tabs it leaves out and where that count is drawn (`width` 0 when none is).
struct TabStrip {
    std::string text;
    std::vector<PlacedTab> tabs;
    std::size_t before = 0; ///< tabs left out to the left, drawn `<N`
    std::size_t after = 0;  ///< ...and to the right, drawn `N>`
    std::int64_t before_first = 0;
    std::int64_t before_width = 0;
    std::int64_t after_first = 0;
    std::int64_t after_width = 0;
    /// The tab a press on a count makes active: the nearest one it leaves out on its side.
    std::size_t before_tab() const noexcept { return before > 0 ? before - 1 : 0; }
    std::size_t after_tab() const noexcept {
        return tabs.empty() ? 0 : tabs.back().index + 1;
    }
};

/// A tab's face: `*word*` when active, `[word]` otherwise.
inline std::string tab_face(std::string_view word, bool active) {
    return active ? "*" + std::string(word) + "*" : "[" + std::string(word) + "]";
}

/// WHAT `columns` SHOW OF `tabs` WITH `active` THE ACTIVE ONE (an index past the end is the
/// first): the active tab always, then as many on each side as fit beside it, grown outward a tab
/// at a time, right before left, with the counts of what is left out spending the same width. A
/// width too narrow for the active tab alone still shows it, cut by whoever draws the row.
inline TabStrip window_tab_strip(const std::vector<std::string>& tabs, std::size_t active,
                                 std::int64_t columns) {
    TabStrip out;
    if (tabs.empty()) {
        return out;
    }
    const std::size_t n = tabs.size();
    if (active >= n) {
        active = 0;
    }
    const auto face = [&tabs](std::size_t i) {
        return static_cast<std::int64_t>(tabs[i].size()) + 2;
    };
    const auto count_width = [](std::size_t hidden) {
        return hidden == 0 ? std::int64_t{0}
                           : static_cast<std::int64_t>(std::to_string(hidden).size()) + 2;
    };
    // The width of tabs `first`..`last` with a space between each, and each count with its space.
    const auto width = [&](std::size_t first, std::size_t last) {
        std::int64_t w = 0;
        for (std::size_t i = first; i <= last; ++i) {
            w += face(i) + (i > first ? 1 : 0);
        }
        return w + count_width(first) + count_width(n - 1 - last);
    };
    std::size_t first = active;
    std::size_t last = active;
    for (bool grew = true; grew;) {
        grew = false;
        if (last + 1 < n && width(first, last + 1) <= columns) {
            ++last;
            grew = true;
        }
        if (first > 0 && width(first - 1, last) <= columns) {
            --first;
            grew = true;
        }
    }
    out.before = first;
    out.after = n - 1 - last;
    std::int64_t at = 0;
    if (out.before > 0) {
        const std::string mark = "<" + std::to_string(out.before);
        out.before_first = at;
        out.before_width = static_cast<std::int64_t>(mark.size());
        out.text += mark + " ";
        at += out.before_width + 1;
    }
    for (std::size_t i = first; i <= last; ++i) {
        if (i > first) {
            out.text += " ";
            ++at;
        }
        const std::string drawn = tab_face(tabs[i], i == active);
        out.tabs.push_back(PlacedTab{i, tabs[i], i == active, at,
                                     static_cast<std::int64_t>(drawn.size())});
        out.text += drawn;
        at += static_cast<std::int64_t>(drawn.size());
    }
    if (out.after > 0) {
        const std::string mark = std::to_string(out.after) + ">";
        out.text += " ";
        ++at;
        out.after_first = at;
        out.after_width = static_cast<std::int64_t>(mark.size());
        out.text += mark;
    }
    return out;
}

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_TAB_STRIP_HPP
