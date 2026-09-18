// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_INSPECTION_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_INSPECTION_SEAM_VOCABULARY_HPP

// ============================================================================================
// THE INSPECTION SEAM -- a PANE as the subject of an inspector (the Info pane), and the one
// thing an inspector may ask to change about it.
// ============================================================================================
//
// WHO OWNS WHAT. A pane's authored placement is the SETUP's (the desk a maker arranged), a
// maker-made pane's regions are its DEFINITION's, and what the screen made of either is the
// host's reading. Every one of those is this host's, so the rows an inspector shows are this
// host's rows -- the same `Row`s, with the same setters, the Pane Manager was built from
// (`pane_subject_rows`). What crosses is a PICTURE of them (`PaneSubjectShown`), and a write
// crosses back as an ask the owner answers (`PaneCommitRequested`). An inspector holds no desk,
// no definition and no setter; it can ask, and be refused in the owner's words.
//
// WHICH PANE IS THE SUBJECT IS THE INSPECTOR'S CHOICE AND NOTHING ELSE'S. The host records it
// (`Session::inspected`) because the rows are closures over the live session and cannot cross,
// and it is written by one door (`InspectPaneRequested`): never by the selection, the pane that
// holds the keys, a press elsewhere or Escape. An inspector may name itself.
//
// ---- A COMMIT NAMES WHAT IT WAS TYPED INTO -------------------------------------------------
//
// A row index names a row of a picture. So the host NAMES what its rows address -- one pane, on
// one desk, with one layout of rows -- publishes the name beside the rows, and writes a commit
// only while the name it carries is still the rows' own. The name moves when the inspector
// chooses another pane, when another desk is put live (a switch, a restore: the same pane's
// Width on another desk is another property), and when the rows' layout changes (a maker-made
// pane's definition opening or closing). It does not move when a value, a room or the pane's
// provider does: a draft outlives those, and a commit that reaches an owner who can no longer
// write it is refused in that owner's words.
//
// ---- SAID WHEN IT CHANGES ------------------------------------------------------------------
//
// Published `to_any` as the office, compared against the last utterance and said only when it
// moved (`StandingConditions`' discipline, and for its reason: a presenter answers a
// publication by saying its rows, a repaint follows, and an unconditional publication would not
// terminate). An inspector that has just arrived asks (`PaneSubjectRequested`) and is answered
// the picture as it is now.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// ONE INSPECTOR ROW, AS THE PANEL SHOWS IT.
///
/// `value` is a FRESH READ through the row's property at the moment the host derived the
/// picture -- there is no cached copy on either side to go stale.
///
/// (This shape was the object document's first (`document_seam_vocabulary.hpp`, which includes
/// this header); its name, version and fields are unchanged.)
struct ShownProperty {
    std::string label;
    std::string value;
    bool editable = false; ///< false for a derived row (a resolved window, a state)
    bool section = false;  ///< a heading inside the list: a label, no value, never editable
    ZEN_SHAPE(ShownProperty, 1, ZEN_FIELD(label), ZEN_FIELD(value), ZEN_FIELD(editable),
              ZEN_FIELD(section));
};

/// MAKE THIS PANE THE INSPECTOR'S SUBJECT -- the one writer of it. Answered `PaneSubjectActed`:
/// accepted, or refused in words when the reference is in neither this build's vocabulary nor
/// the live desk. An office asks; personal speech is answered by nobody.
struct InspectPaneRequested {
    std::string office;
    std::string pane;
    ZEN_SHAPE(InspectPaneRequested, 1, ZEN_FIELD(office), ZEN_FIELD(pane));
};

/// TELL ME THE SUBJECT AS IT IS NOW -- asked by an inspector that has just arrived, and
/// answered `PaneSubjectShown` to that incarnation alone.
struct PaneSubjectRequested {
    ZEN_SHAPE(PaneSubjectRequested, 1);
};

/// THE SUBJECT, AS THIS HOST READS IT: which pane, what the inventory calls it, the name of what
/// the rows address, and the rows. `office` and `pane` both empty is "no subject yet".
struct PaneSubjectShown {
    std::string office;
    std::string pane;
    std::string name;         ///< the inventory's name for it, or its key when nothing offers it
    std::int64_t subject = 0; ///< what `properties` address, named by the host; 0 is none
    std::vector<ShownProperty> properties;
    ZEN_SHAPE(PaneSubjectShown, 1, ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(name),
              ZEN_FIELD(subject), ZEN_FIELD(properties));
};

/// WRITE `text` INTO ROW `row` OF THE SUBJECT `subject` NAMES.
///
/// The name is judged FIRST, before any setter is reached: one that is not the name the host's
/// rows carry now is refused with nothing written and nothing retargeted, however the strings
/// compare. `row` is an index in that picture's `properties`, and `text` is what was typed.
struct PaneCommitRequested {
    std::int64_t subject = 0;
    std::int64_t row = 0;
    std::string text;
    ZEN_SHAPE(PaneCommitRequested, 1, ZEN_FIELD(subject), ZEN_FIELD(row), ZEN_FIELD(text));
};

/// WHAT AN INSPECT OR A COMMIT CAME TO: accepted with an empty `refusal`, or the owner's own
/// words. A picture follows either way, compared and published on the same repaint.
struct PaneSubjectActed {
    bool accepted = false;
    std::string refusal;
    ZEN_SHAPE(PaneSubjectActed, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_INSPECTION_SEAM_VOCABULARY_HPP
