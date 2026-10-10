// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_DOCUMENT_HPP
#define ZENGINE_WORKSHOP_PANE_DOCUMENT_HPP

// A pane's document: the sections of the manual its weave carries (manual/manual.hpp), declared
// beside its offer. Workshop judges a declaration whole under the office stamp and holds it while
// the weave that sent it holds the office; a re-offer, which is how a reloaded image arrives,
// drops it, so the words Workshop holds are always the running image's own. The words decide
// nothing: they are not a grant, and nothing waits on them.
// Reference: manual/docs/manual.md#a-pane-declares-its-manual.

#include "manual/manual.hpp"

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// ONE SECTION OF A PANE'S MANUAL: its key, the heading it stands under, and its words.
struct PaneDocumentSection {
    std::string key;
    std::string group;
    std::string text;
    ZEN_SHAPE(PaneDocumentSection, 1, ZEN_FIELD(key), ZEN_FIELD(group), ZEN_FIELD(text));
    friend bool operator==(const PaneDocumentSection&, const PaneDocumentSection&) = default;
};

/// Provider -> Workshop: the manual of one pane it offered, every section in page order, with the
/// page's content id. Judged whole under the office stamp and refused aloud; a later declaration
/// replaces it, and it counts while the weave that sent it holds the office.
struct PaneDocumentDeclared {
    std::string pane;
    std::string content_id;
    std::vector<PaneDocumentSection> sections;
    ZEN_SHAPE(PaneDocumentDeclared, 1, ZEN_FIELD(pane), ZEN_FIELD(content_id),
              ZEN_FIELD(sections));
};

/// A PANE'S MANUAL AS ITS DECLARATION: every section of the page its weave carries, in order.
inline PaneDocumentDeclared pane_document_of(std::string pane, const manual::Manual& page) {
    PaneDocumentDeclared out;
    out.pane = std::move(pane);
    out.content_id = std::string(page.content_id);
    out.sections.reserve(page.count);
    for (std::size_t i = 0; i < page.count; ++i) {
        const manual::Section& s = page.sections[i];
        out.sections.push_back(
            PaneDocumentSection{std::string(s.key), std::string(s.group), std::string(s.text)});
    }
    return out;
}

/// Words Workshop holds: printable bytes and line breaks, nothing a terminal or a row would
/// read as a control. Bytes past ASCII are taken as they are, for a page in any language.
inline bool pane_document_text_ok(std::string_view text) noexcept {
    for (const char c : text) {
        const auto b = static_cast<unsigned char>(c);
        if ((b < 0x20u && b != '\n') || b == 0x7Fu) {
            return false;
        }
    }
    return true;
}

/// A SECTION'S BOUND, by its key and heading: `Follow` is shorter, and a heard shape's or a shown
/// row's shorter still.
inline std::size_t pane_document_bound(const PaneDocumentSection& s) noexcept {
    if (s.key == "follow") {
        return manual::kMaxManualFollowBytes;
    }
    if (!s.key.empty() && (s.group == "Hears" || s.group == "Shows") && s.key != "hears" &&
        s.key != "shows") {
        return manual::kMaxManualHeardBytes;
    }
    return manual::kMaxManualSectionBytes;
}

/// WHAT IS WRONG WITH A WHOLE DECLARATION, or empty: a content id, at most `kMaxManualSections`
/// sections and `kMaxManualBytes` of words, each key present, bounded, of a key's characters and
/// declared once, and every section's words within its bound and printable.
inline std::string pane_document_declared_problem(const PaneDocumentDeclared& d) {
    if (d.content_id.size() != 64 ||
        d.content_id.find_first_not_of("0123456789abcdef") != std::string::npos) {
        return "a document names its page by the page's SHA-256, sixty-four lowercase hex digits";
    }
    if (d.sections.size() > manual::kMaxManualSections) {
        return "a document holds at most " + std::to_string(manual::kMaxManualSections) +
               " sections -- this holds " + std::to_string(d.sections.size());
    }
    std::size_t total = 0;
    for (std::size_t i = 0; i < d.sections.size(); ++i) {
        const PaneDocumentSection& s = d.sections[i];
        if (s.key.empty() || s.key.size() > manual::kMaxManualKeyBytes ||
            s.key.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "0123456789_.-") != std::string::npos) {
            return "section " + std::to_string(i + 1) + "'s key is not a key: letters, digits, " +
                   "`_`, `.` and `-`, at most " + std::to_string(manual::kMaxManualKeyBytes) +
                   " bytes";
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (d.sections[j].key == s.key) {
                return "section `" + s.key + "` is declared twice";
            }
        }
        const std::size_t bound = pane_document_bound(s);
        if (s.text.size() > bound) {
            return "section `" + s.key + "` holds " + std::to_string(s.text.size()) +
                   " bytes, past " + std::to_string(bound);
        }
        if (!pane_document_text_ok(s.group) || s.group.size() > manual::kMaxManualKeyBytes ||
            !pane_document_text_ok(s.text)) {
            return "section `" + s.key + "` holds a control character";
        }
        total += s.text.size();
    }
    if (total > manual::kMaxManualBytes) {
        return "a document holds at most " + std::to_string(manual::kMaxManualBytes) +
               " bytes of words -- this holds " + std::to_string(total);
    }
    return std::string();
}

/// The section under `key` in a held document, or nullptr.
inline const PaneDocumentSection* find_pane_document_section(const PaneDocumentDeclared& d,
                                                             std::string_view key) noexcept {
    for (const PaneDocumentSection& s : d.sections) {
        if (s.key == key) {
            return &s;
        }
    }
    return nullptr;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_DOCUMENT_HPP
