// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_ATTENTION_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_ATTENTION_SEAM_VOCABULARY_HPP

// WHAT IS TRUE RIGHT NOW, SAID ACROSS THE PANE SEAM -- the arc's ONE new host-to-pane
// sentence, and the only protocol growth since PR #15.
//
// ---- WHY THIS ONE HAD TO EXIST -------------------------------------------------
//
// Every other migrated pane asks the host a question and hears an answer: the browser asks
// where this run began, the Builder asks what the project is waiting on. The Attention pane
// cannot, because EVERY ROW OF IT IS A HOST DERIVATION and none of it is a fact the pane
// could go and get. Two of the conditions are files this host read at boot and could not
// use; three kinds are derived, per repaint, from live host state -- a pane that refused an
// update, a pane the maker authored that no cell of the screen is showing, and the row
// realization is stopped at. There is nothing to ask FOR: what a maker needs to know is
// what the host has just finished working out.
//
// AND A CONDITION HAS NO LIFETIME AN ASK COULD FOLLOW. It "disappears because it resolved,
// never because something else was said" (WL-ATTN-01), and nothing tells the pane when that
// happened. A pane that asked would be a pane that polled; the host says it instead.
//
// ---- SAID WHEN IT CHANGES, AND NOT PER REPAINT ---------------------------------
//
// ⚠ AN UNCONDITIONAL PUBLICATION WOULD NOT TERMINATE, and that is measured rather than
// feared: Workshop repaints when a pane's content arrives (`WorkshopWeave::on(PaneContent)`
// ends in `repaint`), so host publishes -> pane says its rows -> host repaints -> host
// publishes would be a loop with no quiet state in it. So the host compares what it is
// about to say against what it last said and stays silent when they are equal, which is the
// same discipline `builder::BuildStatus` is under and the reason this seam has a quiet
// state at all.
//
// THAT MEMORY IS THE PUBLISHER'S OWN, NOT A SECOND COPY OF THE TRUTH. The host still
// derives every condition per repaint and holds none (WL-ATTN-03, WL-ATTN-04); what it
// remembers is the last thing it SAID, which is a fact about its own speech.
//
// ---- WHAT CROSSES, AND WHAT DOES NOT -------------------------------------------
//
// VALUES, and one of them is a resolved sentence rather than a name. A condition names an
// action by its catalog id and the built-in's painter turned that id into words through the
// EFFECTIVE KEYMAP (`try: <gesture> <label>`) -- a reading no loaded image can make, since
// the keymap is the host's and a maker may have moved the key. So the host resolves it and
// what crosses is the finished sentence. The id itself does not cross, which keeps the law
// that a condition names an action and holds no power exactly as strong as it was: the pane
// is handed words and cannot press anything.
//
// ⚠ AND A CONDITION HAS A WIRE FORM NOW, WHICH IT DID NOT HAVE. `workshop/attention.hpp`
// included exactly `surface/vocabulary.hpp` so that a condition "cannot be observed,
// recorded, selected or persisted" (WL-ATTN-11). The pane's version of that claim is
// narrower and is written down rather than quietly kept: these shapes ride the ordinary
// bus, so a Recorder in this process would see them. What is unchanged is that nothing in
// the condition path holds a timer, a callback or a history, that displaying one still
// writes none, and that no condition is persisted anywhere. The register says so in the
// same words.

#include "surface/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// ONE CONDITION THAT IS TRUE RIGHT NOW, AS A VALUE.
///
/// The first four fields are `Condition`'s own (`workshop/attention.hpp`) and mean exactly
/// what they mean there: `key` is the owner's identity for the fact, `compact` the one line
/// a glance gets, `detail` the sentence under it, `role` its loudness in `surface::role`'s
/// space. The fifth is not a field of `Condition` at all.
struct StandingCondition {
    std::string key;
    std::string compact;
    std::string detail;
    std::int64_t role = surface::role::kAccent;

    /// WHAT A MAKER COULD PRESS, ALREADY IN WORDS -- `try: <gesture> <label>`, composed by
    /// the host against the effective keymap, or empty where the condition names no action.
    /// A NAME would have been the smaller thing to send and the wrong one: the pane cannot
    /// read a keymap, so an id would have reached it as an id and a maker would have been
    /// shown `workshop.manage` where the built-in showed them the key they had bound.
    std::string suggestion;

    ZEN_SHAPE(StandingCondition, 1, ZEN_FIELD(key), ZEN_FIELD(compact), ZEN_FIELD(detail),
              ZEN_FIELD(role), ZEN_FIELD(suggestion));
};

/// EVERY CONDITION THAT IS TRUE RIGHT NOW, IN THE HOST'S OWN ORDER.
///
/// PUBLISHED `to_any` AND NOT ADDRESSED. The host cannot name the Attention pane's office:
/// which weave presents this is the load plan's business and a host that addressed one would
/// be a host with a pane compiled into it again. It is the shape `builder::BuildStatus` is
/// in, one seam over, and anything that can hear it is welcome to -- what it carries is
/// what a maker can already read off the screen.
///
/// ⚠ EMPTY IS A REAL ANSWER AND IS SAID. "Nothing is wrong" is what a healthy Workshop has
/// to be able to tell a pane that is open and looking, and it is also the retraction: the
/// last condition resolving is one publication with no rows in it, exactly as the compact
/// chip's own retraction is one empty `SurfaceText`.
///
/// THE ORDER IS `ranks_before`'s -- loudness, then key, total, with an unknown role last
/// (WL-ATTN-07). It is the host's ranking and it crosses already applied, because the one
/// place this application claims one role is more urgent than another stays one place.
struct StandingConditions {
    std::vector<StandingCondition> rows;
    ZEN_SHAPE(StandingConditions, 1, ZEN_FIELD(rows));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_ATTENTION_SEAM_VOCABULARY_HPP
