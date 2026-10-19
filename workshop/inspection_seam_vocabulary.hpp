// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_INSPECTION_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_INSPECTION_SEAM_VOCABULARY_HPP

// The inspection seam: a pane as the subject of an inspector (the Info pane), and the one change
// an inspector may ask for. The rows are this host's (`pane_subject_rows`): a picture of them
// crosses, and a commit crosses back as an ask the owner answers. The subject is the inspector's
// choice, written by one door. A commit names what it was typed into and is written only while
// that name is still the rows' own. Published when it changes; an arriving inspector asks. v2's
// rows also say how a setting is found -- its tags, its pin, its lock -- for an inspector taking v2.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// One inspector row as the pane shows it; `value` is a fresh read when the picture was derived.
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

namespace v2 {

/// ONE INSPECTOR ROW, as v1 says it, and how an inspector finds a setting among the rows: whether
/// it is a row of the SETTINGS section, the tags its declaration lists it under, its place among
/// the pins, whether a lock holds it, whether it is kept with no declaration taking it, and what
/// it does in one line.
struct ShownProperty {
    std::string label;
    std::string value;
    bool editable = false;
    bool section = false;
    bool setting = false;               ///< a row of the SETTINGS section: a tab filters these alone
    std::vector<std::string> tags = {}; ///< its declaration's tags
    std::int64_t pin = -1;              ///< its place among the pins, from 0; -1 when not pinned
    std::string lock;                   ///< "", or the scope a lock holds it in: "everywhere"
    bool kept = false;                  ///< a stored key no counted declaration takes
    std::string summary;
    ZEN_SHAPE(ShownProperty, 2, ZEN_FIELD(label), ZEN_FIELD(value), ZEN_FIELD(editable),
              ZEN_FIELD(section), ZEN_FIELD(setting), ZEN_FIELD(tags), ZEN_FIELD(pin),
              ZEN_FIELD(lock), ZEN_FIELD(kept), ZEN_FIELD(summary));
    friend bool operator==(const ShownProperty&, const ShownProperty&) = default;
};

/// THE SUBJECT, as v1 says it, over v2's rows, and which pin list a pin act on it edits: the
/// scope `workshop.pinned-scope` names.
struct PaneSubjectShown {
    std::string office;
    std::string pane;
    std::string name;
    std::int64_t subject = 0;
    std::vector<ShownProperty> properties;
    std::string pins;
    ZEN_SHAPE(PaneSubjectShown, 2, ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(name),
              ZEN_FIELD(subject), ZEN_FIELD(properties), ZEN_FIELD(pins));
    friend bool operator==(const PaneSubjectShown&, const PaneSubjectShown&) = default;
};

} // namespace v2

/// v2's subject as v1 says it, for an inspector that takes v1 alone: the rows without how a
/// setting is found.
inline PaneSubjectShown narrow_pane_subject(const v2::PaneSubjectShown& said) {
    PaneSubjectShown out{said.office, said.pane, said.name, said.subject, {}};
    out.properties.reserve(said.properties.size());
    for (const v2::ShownProperty& p : said.properties) {
        out.properties.push_back(ShownProperty{p.label, p.value, p.editable, p.section});
    }
    return out;
}

/// ...and v1's as v2 says it: no row a setting, none pinned, tagged or locked.
inline v2::PaneSubjectShown widen_pane_subject(const PaneSubjectShown& said) {
    v2::PaneSubjectShown out{said.office, said.pane, said.name, said.subject, {}, std::string()};
    out.properties.reserve(said.properties.size());
    for (const ShownProperty& p : said.properties) {
        v2::ShownProperty row;
        row.label = p.label;
        row.value = p.value;
        row.editable = p.editable;
        row.section = p.section;
        out.properties.push_back(std::move(row));
    }
    return out;
}

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
