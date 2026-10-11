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
#include <string>
#include <string_view>
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
    ZEN_SHAPE(PaneDocumentShown, 1, ZEN_FIELD(pane), ZEN_FIELD(section), ZEN_FIELD(content_id),
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
            out.sections.push_back(
                PaneDocumentSection{std::string(s.key), std::string(s.group), std::string(s.text)});
        }
    }
    if (out.sections.empty()) {
        out.refusal = "the manual of " + std::string(page.office) + " has no section `" +
                      ask.section + "`";
    }
    return out;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_DOCUMENT_HPP
