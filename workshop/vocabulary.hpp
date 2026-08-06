// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_VOCABULARY_HPP

// The Workshop package's shapes — the authored material a maker manipulates.
//
// W-0's whole subject is one boring rectangle, and the interesting question is
// not how to draw it but what it IS. So the shapes here are the smallest set
// that can carry an honest answer, and every one of them was cut down to it:
//
//   WorkshopExtent  a width or a height as AUTHORED -- a mode plus an amount,
//                   which is one property, not two (see the note on it).
//   WorkshopRect    one authored rectangle: an identity, a display name, a
//                   position, and two extents.
//   WorkshopDoc     the authored content, and nothing else. The weave's state.
//
// WHAT IS DELIBERATELY ABSENT, so the absence is a decision rather than an
// oversight:
//
//   - no parent/child. A hierarchy of one level (the document holds
//     rectangles) is the hierarchy W-0 actually needs to name and select an
//     object; nesting is W0 work with its own evidence to produce.
//   - no colour. It would be a third semantic property type and it buys the
//     phase nothing the extent does not already buy (the canvas roles
//     already prove medium-agnostic ink; see surface/vocabulary.hpp).
//   - no z / no ordering field. Paint order is list order, once, in
//     SurfaceCanvas.
//   - no persisted anything. W-0 does not save (the phase's explicit
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
// somewhere less visible.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// How an extent was authored. Two spellings of one intent, and the reason
/// `WorkshopExtent` exists at all rather than a bare number.
inline constexpr std::int64_t kExtentCells = 0;   ///< an absolute count of canvas cells
inline constexpr std::int64_t kExtentPercent = 1; ///< a share of the workspace, 0..100

/// A width or a height AS AUTHORED — one property carrying both halves.
///
/// This is the shape the old builder's inspector got wrong twice over, and both
/// mistakes are worth naming because avoiding them is most of what W-0 learned:
///
///   1. It presented "Width Type" and "Width Value" as two rows, because that
///      is how the two were STORED. A maker does not author a type and then
///      author a value; they author a width. One property, one row, one commit.
///   2. It let a resolved number stand in for the authored one. `70%` and `33
///      cells` are different facts about the same rectangle, and only the first
///      is the property -- the second is what the current workspace makes of it
///      (see document.hpp's resolve()). An inspector that shows only the second
///      has silently thrown the maker's intent away.
///
/// `amount` means cells when `mode == kExtentCells` and percent when it is
/// kExtentPercent. Nothing here validates: what a legal extent IS belongs to
/// the operation that accepts one (document.hpp), because that is the only
/// place that can also refuse.
struct WorkshopExtent {
    std::int64_t mode = kExtentCells;
    std::int64_t amount = 0;

    friend bool operator==(const WorkshopExtent&, const WorkshopExtent&) = default;

    ZEN_SHAPE(WorkshopExtent, 1, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

/// One authored rectangle.
///
/// `id` IS the identity; `name` is a label for a human and nothing more. They
/// are separate fields on purpose: the old builder used the name as both, so two
/// rectangles could not share a name and renaming one silently renamed what
/// referred to it. Here two rectangles may be called the same thing and still be
/// different objects, which is the whole point of having an id -- and W-0 proves
/// it by doing exactly that in the suite.
///
/// What `id` is NOT: durable. It is minted by the document that holds it
/// (WorkshopDoc::next_id) and means nothing outside that document's lifetime.
/// W-0 does not persist, so nothing yet claims otherwise; the moment it does,
/// the identity question is a real one and not this phase's to answer.
struct WorkshopRect {
    std::int64_t id = 0;
    std::string name;
    std::int64_t x = 0; ///< canvas cells, from the workspace's top-left
    std::int64_t y = 0;
    WorkshopExtent width;
    WorkshopExtent height;

    ZEN_SHAPE(WorkshopRect, 1, ZEN_FIELD(id), ZEN_FIELD(name), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(width), ZEN_FIELD(height));
};

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
    std::vector<WorkshopRect> rects;
    std::int64_t next_id = 1; ///< the identity mint; document-owned, so it rides with the document

    ZEN_EXPOSE();
    ZEN_SHAPE(WorkshopDoc, 1, ZEN_FIELD(rects), ZEN_FIELD(next_id));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_VOCABULARY_HPP
