// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_VOCABULARY_HPP

// The Workshop package's shapes — the authored material a maker manipulates.
//
// The interesting question about a maker's rectangle is not how to draw it but
// what it IS -- and whose vocabulary it is spelled in. It is not Workshop's:
// what a maker authors (an identity, a label, a place, two extents) is what ANY
// visual Zengine application authors, so the TYPE is `zengine::ui::Element` in
// the UI package (README.md#ui--the-authoredresolved-vocabulary) and this file
// is the document that HOLDS such elements rather than the file that invents
// them.
//
// So what is here is exactly Workshop's own:
//
//   WorkshopDoc     the authored content, and nothing else. The weave's state:
//                   a sequence of zengine::ui::Element, plus the identity mint
//                   that makes them distinguishable.
//
// There is deliberately no alias, no wrapper and no forwarding header for the
// shared types. Workshop names `ui::Element` and `ui::Extent` at every call
// site, so a reader sees whose concept it is where the code is read rather than
// behind a spelling that suggests Workshop still owns it.
//
// WHAT IS DELIBERATELY ABSENT, so the absence is a decision rather than an
// oversight:
//
//   - no parent/child, and the absence is a measured claim rather than a
//     deferral. An element says what its values are measured against
//     (ui::Element::context), which is what composition actually needed; it
//     says nothing about containment, ownership, clipping, painting or
//     lifetime. Workshop authors ONE policy over that -- a source with
//     dependents is not deletable (document.hpp) -- and that policy is
//     Workshop's, in Workshop's document law, not a property of the
//     relationship. "Put B inside A" remains a thing an application could
//     build; it is not a thing this document does.
//   - no colour. It would be a third semantic property type and it buys the
//     phase nothing the extent does not already buy (the canvas roles
//     already prove medium-agnostic ink; see surface/vocabulary.hpp).
//   - no z / no ordering field. Paint order is list order, once, in
//     SurfaceCanvas — and now also in ui::Scene, which says the same thing
//     about the same sequence. That sequence is a PERSISTED fact, because it is
//     semantically observable four ways (paint order, which object a click
//     finds under an overlap, the object list, and where the selection lands
//     after a delete) — but it is still the order of the vector and not a field
//     on an element.
//   - no field whose only purpose is to survive a save. Persistence added
//     nothing to this struct: what a maker authored was already all of it, and
//     the FILE's shape lives in persist.hpp, separate on purpose (see the note
//     there).
//
// THE STATE IS PUBLIC AND THAT IS THE SUBSTRATE'S REQUIREMENT, NOT A CHOICE.
// ZEN_SHAPE registers members by pointer-to-member (`&ZenSelf::member`), so a
// weave's state fields must be public — which means Workshop CANNOT make its
// semantic setters the only door to its own invariants. It can only make them
// the only door Workshop itself walks through (see document.hpp: every write in
// this package goes through a function that can refuse). Reported as pressure
// rather than hidden behind a wrapper that would merely move the public members
// somewhere less visible. One consequence is the UI package's: because a poke
// can write any int64 into an authored extent, `ui::resolve_extent` has to be
// total for values no setter would ever have accepted.

#include "ui/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <vector>

namespace zengine::workshop {

/// The authored content — the document, and only the document.
///
/// What is NOT here is as load-bearing as what is: the SELECTION, the workspace
/// extent, and every editor draft are session facts, not authored content, and
/// they live as plain members of the weave (weave.hpp) exactly the way a Skin's
/// `announced_` flag does. The split is structural — the two kinds of fact
/// cannot be confused because they are not in the same struct.
///
/// THE PERSISTENCE BOUNDARY IS EXACTLY THIS STRUCT: everything in it survives a
/// process, nothing outside it does. The sharpest case is the workspace extent
/// — the one session fact a save would be most tempted to keep, and keeping it
/// would destroy the proof that a share is authored rather than resolved (load
/// the same file into a narrower workspace and the authored `60%` is unchanged
/// while the resolved cells are not).
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
