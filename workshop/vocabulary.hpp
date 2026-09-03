// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_VOCABULARY_HPP

// The Workshop package's shapes — the authored material a maker manipulates.
// Workshop law: agents/workshop/document.md

#include "ui/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <vector>

namespace zengine::workshop {

/// The authored content — the document, and only the document.
// WL-DOC-13 -- agents/workshop/document.md
struct WorkshopDoc {
    std::vector<ui::Element> elements;
    std::int64_t next_id = 1; ///< the identity mint; document-owned, so it rides with the document

    /// Two documents are the same document when they hold the same objects in
    /// the same order with the same mint. Used by the weave to answer "is what
    /// is on screen what is on disk" by COMPARING rather than by a dirty flag —
    /// a flag needs a hand at every write site and is wrong the first time one
    /// is missed, while a comparison cannot drift from the thing it describes.
    friend bool operator==(const WorkshopDoc&, const WorkshopDoc&) = default;

    ZEN_EXPOSE();
    /// Version 2, though its OWN two fields have never changed. What changed is
    /// `ui::Element`, which went to v2 when it grew a context -- and a schema's
    /// content-id is derived from the WHOLE shape, so this one changed with it.
    /// A version that claimed otherwise would be saying "the same shape" about a
    /// different shape.
    ZEN_SHAPE(WorkshopDoc, 2, ZEN_FIELD(elements), ZEN_FIELD(next_id));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_VOCABULARY_HPP
