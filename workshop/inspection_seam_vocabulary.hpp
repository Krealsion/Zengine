// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_INSPECTION_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_INSPECTION_SEAM_VOCABULARY_HPP

// The inspection seam: a pane as the subject of an inspector (the Info pane), and the one change
// an inspector may ask for. The rows are this host's (`pane_subject_rows`): a picture of them
// crosses, and a commit crosses back as an ask the owner answers. The subject is the inspector's
// choice, written by one door. A commit names what it was typed into and is written only while
// that name is still the rows' own. Published when it changes; an arriving inspector asks.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// One inspector row as the panel shows it; `value` is a fresh read when the picture was derived.
struct ShownProperty {
    std::string label;
    std::string value;
    bool editable = false; ///< false for a derived row (a resolved window, a state)
    bool section = false;  ///< a heading inside the list: a label, no value, never editable
    ZEN_SHAPE(ShownProperty, 1, ZEN_FIELD(label), ZEN_FIELD(value), ZEN_FIELD(editable),
              ZEN_FIELD(section));
};

/// Make this pane the inspector's subject -- the one writer of it. Refused in words when the
/// reference is in neither this build's vocabulary nor the live desk; only an office is answered.
struct InspectPaneRequested {
    std::string office;
    std::string pane;
    ZEN_SHAPE(InspectPaneRequested, 1, ZEN_FIELD(office), ZEN_FIELD(pane));
};

/// Ask for the subject as it is now, answered to the asking incarnation alone.
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

/// Write `text` into row `row` of the subject `subject` names. The name is judged first: one that
/// is not the rows' current name is refused, with nothing written or retargeted.
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
