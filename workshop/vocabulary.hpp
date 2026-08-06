// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_VOCABULARY_HPP

// The Workshop package's shapes — the authored material a maker manipulates.
//
// W-0's whole subject is one boring rectangle, and the interesting question is
// not how to draw it but what it IS. W-1 answered a further question about the
// same rectangle: whose vocabulary is it? The answer turned out not to be
// Workshop's. What a maker authors -- an identity, a label, a place, and two
// extents -- is what ANY visual Zengine application authors, so it moved out to
// the UI package, and this file is now the document that HOLDS such elements
// rather than the file that invents them.
//
// So what is left here is exactly Workshop's own:
//
//   WorkshopDoc     the authored content, and nothing else. The weave's state:
//                   a sequence of zengine::ui::Element, plus the identity mint
//                   that makes them distinguishable.
//
// and what left, with the phase that moved it:
//
//   WorkshopExtent  -> zengine::ui::Extent   (W-1)
//   WorkshopRect    -> zengine::ui::Element  (W-1; `name` is spelled `label`
//                                             there, because a label is what it
//                                             always was -- see the id note)
//
// There is deliberately no alias, no wrapper and no forwarding header for
// either. Workshop names `ui::Element` at every call site, so the move is
// visible where the code is read rather than hidden behind a spelling that
// suggests Workshop still owns the concept.
//
// WHAT IS DELIBERATELY ABSENT, so the absence is a decision rather than an
// oversight:
//
//   - no parent/child. A hierarchy of one level (the document holds
//     elements) is the hierarchy Workshop actually needs to name and select an
//     object; nesting is W0 work with its own evidence to produce, and it is
//     now a named seam in the UI package rather than a gap here.
//   - no colour. It would be a third semantic property type and it buys the
//     phase nothing the extent does not already buy (the canvas roles
//     already prove medium-agnostic ink; see surface/vocabulary.hpp).
//   - no z / no ordering field. Paint order is list order, once, in
//     SurfaceCanvas — and now also in ui::Scene, which says the same thing
//     about the same sequence.
//   - no persisted anything. Workshop does not save (the phase's explicit
//     non-goal), and the shapes are honest about that by carrying nothing
//     whose only purpose would be to survive a save.
//
// THE STATE IS PUBLIC AND THAT IS THE SUBSTRATE'S REQUIREMENT, NOT A CHOICE.
// ZEN_SHAPE registers members by pointer-to-member (`&ZenSelf::member`), so a
// weave's state fields must be public — which means Workshop CANNOT make its
// semantic setters the only door to its own invariants. It can only make them
// the only door Workshop itself walks through (see document.hpp: every write in
// this package goes through a function that can refuse). Reported as pressure
// rather than hidden behind a wrapper that would merely move the public members
// somewhere less visible. One consequence became the UI package's problem in
// W-1 and was fixed there: because a poke can write any int64 into an authored
// extent, `ui::resolve_extent` has to be total for values no setter would ever
// have accepted.

#include "ui/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <vector>

namespace zengine::workshop {

/// The authored content — the document, and only the document.
///
/// What is NOT here is as load-bearing as what is: the SELECTION, the workspace
/// extent, and every editor draft are session facts, not authored content, and
/// they live as plain members of the weave (workshop.cpp) exactly the way a
/// Skin's `announced_` flag does. W-0 has no persistence boundary to test that
/// split against, so the split is made structurally instead — the two kinds of
/// fact cannot be confused because they are not in the same struct.
///
/// ZEN_EXPOSE(): every field is poke-manipulable, deliberately and in the open.
/// The document holds no secrets, live manipulation is the substrate's point,
/// and a maker tool that hid its own material would be an odd thing to build
/// here. Note what this costs, honestly: a poke writes the struct directly and
/// therefore bypasses every refusal in document.hpp. That is the substrate
/// working as designed (poke is the operator's door, not the application's), and
/// it is why the invariants live in the operations a maker's edits actually go
/// through rather than being claimed as properties of the data.
struct WorkshopDoc {
    std::vector<ui::Element> elements;
    std::int64_t next_id = 1; ///< the identity mint; document-owned, so it rides with the document

    ZEN_EXPOSE();
    ZEN_SHAPE(WorkshopDoc, 1, ZEN_FIELD(elements), ZEN_FIELD(next_id));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_VOCABULARY_HPP
