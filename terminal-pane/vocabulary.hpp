// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TERMINAL_PANE_VOCABULARY_HPP
#define ZENGINE_TERMINAL_PANE_VOCABULARY_HPP

// The Terminal pane's DURABLE NAMES -- the office it holds, the one pane it offers, the
// action ids its keys answer to, and the state a same-shape reload keeps.
//
// WHY THE OFFICE IS NOT `zengine.workshop`. This pane presents a participant the HOST mounted
// and holds, so the host's office would have looked like the honest address -- and it is
// exactly the address a migrated pane may not have: `zengine.workshop` is the host's
// singleton role and admission refuses a pane offered by whoever holds it as a forgery
// (WL-CAT-03). Info's migration wrote this paragraph first; this is the second pane it is
// true of, and for the same reason.
//
// ⚠ AND THE OFFICE IS NOT THE PARTICIPANT'S EITHER. The terminal participant is a `loom::
// Weave` with an identity and a grant of its own (`workshop.cpp`); this pane is a different
// weave with a different identity and no grant to say anything to a Skin. A maker types here
// and the participant speaks there -- two identities, and the whole value of the seat is
// that they stay two.
//
// WHY THERE IS NO RETIRED `PaneRef` TO CONVERT, and this is the first migration that has
// none. The browser, the Builder and Info were PANELS: a `panel::k*` kind, a place in the
// catalog, a `PaneRef` a saved setup could name -- so each needed a row in
// `workshop/pane_migration.hpp` to move a maker's desk. The Terminal was never a panel. It
// was `Session::terminal.open`, a bool behind a global chord, drawn on a plane after every
// pane; no setup ever named it, no arrangement ever placed it, and there is nothing in any
// file a maker has written for a conversion to rewrite. What a maker gets instead is a pane
// their setup has never heard of -- which the shipped default plan opens for them, and which
// their own saved desks simply do not mention.
//
// WHY THE FIVE ACTION IDS ARE THE BUILT-IN'S OWN SPELLINGS. `terminal.submit`,
// `terminal.back`, `terminal.previous`, `terminal.next` and `terminal.complete` were
// `KeyContext::kTerminal`'s rows in `workshop/keymap.hpp`, on Return, Escape, Up, Down and
// Tab. They are this pane's `PaneActionRow`s now, spelled and defaulted exactly as they
// were, so a maker who authored an override for one keeps it. Nothing collides: the host's
// remaining rows are in other contexts, and the id law refuses a pane claiming one of
// Workshop's own -- which none of these is any more, because all five left with the mode.

#include <zen/weave/shape.hpp>

#include <cstdint>

namespace zengine::terminal_pane {

/// THE OFFICE THIS PANE HOLDS -- the durable half of its `PaneRef`, and the only address
/// anything reaches it by. A ROLE, so Workshop's `PaneRoom` finds whoever holds it rather
/// than the incarnation that offered first.
inline constexpr const char* kTerminalPaneRole = "zengine.terminal";

/// THE PANE KEY, in this office's namespace.
inline constexpr const char* kTerminalPane = "terminal";

/// THE TWO LINES A MAKER READS IN THE PICKER, and what Workshop's pane header says after the
/// office.
inline constexpr const char* kTerminalPaneName = "Terminal";
inline constexpr const char* kTerminalPaneSummary = "talk to the weaves on this bus";

/// THE LIBRARY STEM A HOST BOOTS. Not part of the pane protocol and not durable the way the
/// office is -- it is a file name, here because the host's boot plan and the suite's loader
/// must agree on it.
inline constexpr const char* kTerminalPaneStem = "zengine-terminal-pane";

// ---- THE ACTIONS THE PANE DECLARES (`PaneActions`, workshop/pane_vocabulary.hpp) ---------

inline constexpr const char* kActionSubmit = "terminal.submit";     ///< author the line
inline constexpr const char* kActionBack = "terminal.back";         ///< dismiss list / clear
inline constexpr const char* kActionUp = "terminal.previous";       ///< completion up
inline constexpr const char* kActionDown = "terminal.next";         ///< ...and down
inline constexpr const char* kActionComplete = "terminal.complete"; ///< "help me here"

/// THE STATE A SAME-SHAPE RELOAD KEEPS (RELOAD-1).
///
/// ONE FIELD, AND IT IS THE LINE THE MAKER IS HALF-WAY THROUGH TYPING.
///
/// ⚠ THIS IS THE OPPOSITE CHOICE FROM INFO'S AND THE BUILDER'S, AND FOUR QUESTIONS DECIDE
/// IT -- not the word "composition", which names a real difference and settles nothing on its
/// own. Info kept a cursor and dropped its property draft; the Builder dropped its role line.
/// Two of the four separate a terminal line from those, and TWO DO NOT -- which is the part
/// the first version of this comment got wrong, and it is written out because a rule that
/// only ever agrees with the decision it was written for is not a rule:
///
///   recoverable?      NO, AND NEITHER IS A PROPERTY DRAFT. Reopening a property gives back
///                     its COMMITTED value, never the edits. What differs is how far back a
///                     maker lands, not whether their edits survive. DOES NOT SEPARATE.
///   how much work?    MINUTES, against seconds. An address, a shape, a version and a run of
///                     named arguments, assembled with the completer. SEPARATES, by degree.
///   names a target?   YES -- `#12` and `@office` are targets, and this was written down as
///                     "no". They resolve AT SUBMIT, and a send that does not resolve is
///                     recorded as a refusal on the transcript (`submit_terminal_line`,
///                     read to check this) -- but a role resolves to whoever holds it THEN,
///                     so a kept line can reach a successor. DOES NOT SEPARATE; it is a
///                     named consequence of keeping one.
///   kept, what risks? NOTHING UNTIL AN EXPLICIT SUBMIT: the text sits inert on a row the
///                     maker reads. A property draft's commit writes into a live document.
///                     SEPARATES, and it is the one that carries the decision.
///
/// So the line is kept -- on the effort and on the inertness, not on four clean answers -- and
/// this pane is the first that keeps a draft. A draft whose answers are MIXED is undecided
/// here and in the record (`agents/decisions/the-terminal-is-a-participant.md`); the first one
/// that appears decides it.
///
/// AND THE CARET IS NOT KEPT WITH IT. `component::TextBox` holds the text, the caret, the
/// selection and the window; only the TEXT crosses a reload, and the new image places the
/// caret at the end of it -- which is where the completer requires it to be, and where a
/// maker who is about to keep typing wants it. A caret index carried into an image that may
/// resolve the line differently is a position without the thing that made it mean something.
///
/// AND NOTHING ELSE IS HERE. The transcript is the host's reading, re-said the moment it
/// changes; the completion is derived from the line by an ask; the dismissal and the "asked"
/// flag are about a keystroke a maker made against a word they are no longer typing. A
/// reload that kept those would be a second owner of facts this pane does not own
/// (`agents/decisions/a-presentation-owns-no-facts.md`).
struct TerminalPaneState {
    std::string line; ///< what the maker had typed and not yet submitted
    ZEN_SHAPE(TerminalPaneState, 1, ZEN_FIELD(line));
};

} // namespace zengine::terminal_pane

#endif // ZENGINE_TERMINAL_PANE_VOCABULARY_HPP
