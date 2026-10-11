// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_MANUAL_MANUAL_HPP
#define ZENGINE_MANUAL_MANUAL_HPP

// A weave's manual as the weave carries it: the page `cmake/ZengineManual.cmake` split at its
// headings and compiled into the weave, one section per key, with the page's content id. The page
// is the source; this is a read-only view of the bytes the build embedded from it, so the words a
// running weave says are the words of the page at the commit it was built from.
// Reference: manual/docs/manual.md.

#include <cstddef>
#include <string_view>

namespace zengine::manual {

// ---- The bounds a page is judged by --------------------------------------------------------
//
// The splitter reads these by name from this file, so a page the build accepts is one Workshop
// admits; Workshop judges a declared document by the same constants.

/// The longest section key: an action id's bound, or a shape's name.
inline constexpr std::size_t kMaxManualKeyBytes = 64;
/// The longest section: the lead, `About`, `Use`, a command's section, a group's opening words.
inline constexpr std::size_t kMaxManualSectionBytes = 2048;
/// The longest `Follow`.
inline constexpr std::size_t kMaxManualFollowBytes = 1024;
/// The longest section under `Hears` or `Shows`.
inline constexpr std::size_t kMaxManualHeardBytes = 512;
/// The most sections one page holds.
inline constexpr std::size_t kMaxManualSections = 128;
/// The longest page, in bytes, and so the most words one declaration carries.
inline constexpr std::size_t kMaxManualBytes = 65536;

/// ONE SECTION OF A PAGE: its key (`about`, `use`, `follow`, a command id, a shape's name, a row's
/// key, or the slug of a group heading for the words that open that group), the `##` heading it
/// stands under (empty for the lead), and its Markdown text without its heading.
struct Section {
    std::string_view key;
    std::string_view group;
    std::string_view text;
};

/// A PAGE AS A WEAVE CARRIES IT: the office its title names, the name a weaver sees, the page's
/// content id (the SHA-256 of its bytes, line endings as LF) and its sections in page order.
struct Manual {
    std::string_view office;
    std::string_view name;
    std::string_view content_id;
    const Section* sections = nullptr;
    std::size_t count = 0;

    /// The section under `key`, or nullptr. Keys are unique within a page.
    constexpr const Section* find(std::string_view key) const noexcept {
        for (std::size_t i = 0; i < count; ++i) {
            if (sections[i].key == key) {
                return &sections[i];
            }
        }
        return nullptr;
    }
};

/// Is this the group of a section describing a command? `Commands`, and `Commands in <mode>` or
/// `Commands on <mode>`.
constexpr bool is_command_group(std::string_view group) noexcept {
    return group == "Commands" || group.substr(0, 12) == "Commands in " ||
           group.substr(0, 12) == "Commands on ";
}

/// Is this the group of a section describing a shape an office hears without being commanded?
constexpr bool is_heard_group(std::string_view group) noexcept { return group == "Hears"; }

} // namespace zengine::manual

#endif // ZENGINE_MANUAL_MANUAL_HPP
