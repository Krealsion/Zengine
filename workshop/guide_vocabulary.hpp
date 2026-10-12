// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_GUIDE_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_GUIDE_VOCABULARY_HPP

// A guest points at one control or part of the desk for the weaver to see, and changes nothing
// else: a guide mark, named as a press names it -- a pane, `<office>/<pane>`, and a control's action
// id, drawn by the part `control:<id>`, or a subject `part:<name>` -- with its words beside the
// guest's row name. Granted by the guests file's `guide` power, within the row's `guide` doors.
// How it is drawn and how it ends is the law's: agents/workshop/guide.md.

#include <zen/weave/shape.hpp>

#include <cstddef>
#include <string>

namespace zengine::workshop {

/// Guest -> Workshop: mark `control` of the pane `pane` for the weaver, with `words` beside the
/// guest's own name. `control` is an action id, or `part:<name>` for a subject part; empty clears
/// the asker's mark.
struct GuideRequested {
    std::string pane;    ///< "<office>/<pane>"
    std::string control; ///< "builder.build", or "part:pane:zengine.builder-pane/builder"
    std::string words;   ///< at most `kGuideWordsBytes` of printable ASCII
    ZEN_SHAPE(GuideRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(control), ZEN_FIELD(words));
};

/// Workshop -> the asker, answered at the ask and again when its mark ends: whether the mark stands
/// now, and what Workshop said about it -- where it stands and whether its label shows, the weaver's
/// key for a control with no part, or why it was refused or ended.
struct GuideAnswered {
    bool shown = false;
    std::string said;
    ZEN_SHAPE(GuideAnswered, 1, ZEN_FIELD(shown), ZEN_FIELD(said));
};

/// The most bytes of a mark's words, all printable ASCII.
inline constexpr std::size_t kGuideWordsBytes = 60;
/// The most marks the desk holds at once, one a session.
inline constexpr std::size_t kGuideMarks = 16;
/// How a subject part is named in `control`.
inline constexpr const char* kGuidePartPrefix = "part:";

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_GUIDE_VOCABULARY_HPP
