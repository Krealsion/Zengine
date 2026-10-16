// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_DOCUMENT_HPP
#define ZENGINE_WORKSHOP_PANE_DOCUMENT_HPP

// A pane's manual, asked of the pane: the sections of the page its weave carries
// (manual/manual.hpp), answered to the one who asked. Nothing holds a pane's manual -- not Workshop,
// not the host -- so a reloaded image answers its own words and no copy outlives the image.
// Reference: manual/docs/manual.md#a-pane-answers-its-manual.

#include "manual/manual.hpp"

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// ONE KEY MARKER IN A SECTION'S WORDS, as the build found it (`manual::KeyMark`): the action id it
/// names, where the whole marker stands in `text` (its byte offset and length), and the default key
/// written in it. A reader shows a weaver's own key in the marker's place from these alone.
struct PaneDocumentKey {
    std::string id;
    std::int64_t at = 0;
    std::int64_t length = 0;
    std::string written;
    ZEN_SHAPE(PaneDocumentKey, 1, ZEN_FIELD(id), ZEN_FIELD(at), ZEN_FIELD(length),
              ZEN_FIELD(written));
    friend bool operator==(const PaneDocumentKey&, const PaneDocumentKey&) = default;
};

/// ONE SECTION OF A PANE'S MANUAL: its key, the heading it stands under, its words, and the key
/// markers in them, in order.
struct PaneDocumentSection {
    std::string key;
    std::string group;
    std::string text;
    std::vector<PaneDocumentKey> keys;
    ZEN_SHAPE(PaneDocumentSection, 2, ZEN_FIELD(key), ZEN_FIELD(group), ZEN_FIELD(text),
              ZEN_FIELD(keys));
    friend bool operator==(const PaneDocumentSection&, const PaneDocumentSection&) = default;
};

/// Anyone -> a pane's office: the manual of one pane it offers, the section under `section`, or
/// the whole page when `section` is empty. Answered to the asker alone.
struct PaneDocumentRequested {
    std::string pane;
    std::string section;
    ZEN_SHAPE(PaneDocumentRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(section));
};

/// A pane's office -> the asker: the sections asked for, in page order, and the content id of the
/// page they come from; or a refusal naming what is missing, with no sections.
struct PaneDocumentShown {
    std::string pane;
    std::string section;
    std::string content_id;
    std::vector<PaneDocumentSection> sections;
    std::string refusal;
    ZEN_SHAPE(PaneDocumentShown, 2, ZEN_FIELD(pane), ZEN_FIELD(section), ZEN_FIELD(content_id),
              ZEN_FIELD(sections), ZEN_FIELD(refusal));
};

/// THE ANSWER A PANE GIVES `ask` about the pane it offers as `pane`, from the page it carries: a
/// pane answers in one line, `mail.answer(pane_document(ask, kMyPane, kManual))`.
inline PaneDocumentShown pane_document(const PaneDocumentRequested& ask, std::string_view pane,
                                       const manual::Manual& page) {
    PaneDocumentShown out;
    out.pane = ask.pane;
    out.section = ask.section;
    if (ask.pane != pane) {
        out.refusal = std::string(page.office) + " offers no pane `" + ask.pane + "`";
        return out;
    }
    out.content_id = std::string(page.content_id);
    for (std::size_t i = 0; i < page.count; ++i) {
        const manual::Section& s = page.sections[i];
        if (ask.section.empty() || s.key == ask.section) {
            PaneDocumentSection said{std::string(s.key), std::string(s.group),
                                     std::string(s.text), {}};
            for (std::size_t m = 0; m < s.mark_count; ++m) {
                const manual::KeyMark& mark = s.marks[m];
                said.keys.push_back(PaneDocumentKey{std::string(mark.id),
                                                    static_cast<std::int64_t>(mark.at),
                                                    static_cast<std::int64_t>(mark.length),
                                                    std::string(mark.key)});
            }
            out.sections.push_back(std::move(said));
        }
    }
    if (out.sections.empty()) {
        out.refusal = "the manual of " + std::string(page.office) + " has no section `" +
                      ask.section + "`";
    }
    return out;
}

} // namespace zengine::workshop

namespace loom {
template <class... S>
struct Accept;
template <class... S>
struct Emit;
} // namespace loom

namespace zengine::workshop {

/// THE FACET A PANE OFFERING ONE PANE ANSWERS ITS MANUAL BY, composed with `zengine::With`
/// (activation/activation.hpp): it accepts `PaneDocumentRequested` and answers the asker from
/// `Page` about the pane offered as `Pane`, so the weave lists neither shape and writes no handler.
template <const char* const& Pane, const manual::Manual& Page>
struct PaneManualFacet {
    using Accept = loom::Accept<PaneDocumentRequested>;
    using Emit = loom::Emit<PaneDocumentShown>;

    template <class MailT>
    void on(const PaneDocumentRequested& ask, MailT& mail) {
        (void)mail.answer(pane_document(ask, Pane, Page));
    }
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_DOCUMENT_HPP
