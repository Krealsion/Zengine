// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP

// THE WHOLE PROTOCOL BETWEEN WORKSHOP AND A WEAVE THAT OFFERS IT A PANE (WP-0,
// widened once by SEL-0 and once by MSG-0). Seven shapes, and there is not an eighth.
//
//     PaneCatalogRequested   Workshop  ->  everyone   "who has panes?"
//     PaneOffered            provider  ->  Workshop   "I have this one."
//     PaneRoom               Workshop  ->  provider   "here is how much prose it gets."
//     PaneContent            provider  ->  Workshop   "here is what it says."
//     PanePressed            Workshop  ->  provider   "a maker pressed here, in that room."
//     PaneKey                Workshop  ->  provider   "a key went down, and you have the keyboard."
//     PaneTextInput          Workshop  ->  provider   "...and the platform made this text of it."
//
// THE FIFTH IS THE FOURTH'S BUDGET READ BACKWARDS, which is what made it the one
// worth adding. `PaneRoom` grants a lattice of prose rows and columns; `PanePressed`
// names a place IN that lattice and says nothing else. Workshop already owns the
// geometry that answers WHICH pane a press landed on -- `bounds_of` -> `contains`,
// the same rectangle the painter used -- so the synchronous half of the question
// never crosses the wire and no `consumed` ever comes back (WP-R0). A press that
// reaches a provider was consumed at the Workshop boundary before it was sent.
//
// THE SIXTH AND SEVENTH ARE THE FIRST SHAPES THAT CARRY NO PLACE AT ALL, which is
// exactly what makes them a pair rather than one shape with a `text` field. They
// are `zengine::input`'s own two facts, forwarded: a key transition may produce no
// text (an arrow, Return, Escape) and text may arrive with no key this application
// can name (`%` on most layouts). Input keeps them apart for that reason and this
// seam keeps them apart for the same one -- a single shape would have to spell one
// of the two absences, and there is no honest spelling for "no key".
//
// ---- THERE IS NO PROVIDER FIELD, AND THAT IS THE POINT ----------------------
//
// `PaneOffered` and `PaneContent` say WHICH PANE and never WHOSE. The provider
// half of the durable `PaneRef` (setup.hpp) is `mail.authored_role()` -- the
// office Loom verified at the moment the sentence was authored, carried as
// DELIVERY PROVENANCE that no payload can write and no sender can choose
// (MSG-07). A `provider` field here would be a second answer to a question that
// already has one, and the second answer is the forgeable one: any weave granted
// the shape could name somebody else's office in it and Workshop would have no
// way to tell.
//
// So the absence is enforcement rather than economy. There is nothing to compare
// against the stamp, because there is nothing to compare.
//
// WHAT A LOOM ROLE PROVES, EXACTLY. That the sender held this office at the
// moment it spoke, on this bus, in this process. It is a LIVE, REPLACEMENT-STABLE
// SERVICE ROUTE. It is NOT a package author, a signature, a publisher, a
// marketplace identity, or evidence that the same author came back after a
// restart. Nothing in WP-0 claims any of those and nothing here may grow to.
//
// ---- WHAT PANEROOM IS NOT ---------------------------------------------------
//
// A budget of PROSE ROWS AND COLUMNS, resolved by Workshop through the one
// `surface::fit_region` call every other bounded region in this application goes
// through. Not cells, not pixels, not an extent, not a font, not a rectangle, and
// not the identity of the medium that answered. A provider that knew any of those
// could compute a second layout, and two parties measuring one region is the
// defect `SurfaceTextRegion`'s own doc comment exists to forbid.
//
// ---- WHAT PANECONTENT IS NOT ------------------------------------------------
//
// `surface::SurfaceTextRow` values, reused directly rather than mirrored into a
// parallel row type -- so a provider's row carries the same semantic `role` and
// `background` every first-party row does, and the Skin's palette answers for it
// unchanged. A provider supplies no `SurfaceRect`, no `SurfaceLabel`, no
// `SurfaceTextRegion`, no coordinate, no z-order, no viewport and no caret.
// Workshop assembles ONE `SurfaceCanvas`; nothing here is a second publisher.
//
// ---- WHAT PANEPRESSED IS NOT ------------------------------------------------
//
// A PRIMARY PRESS, and the shape says so by having nowhere to say anything else.
// No button field, no pressed/released field, no modifier field, no timestamp and
// no gesture identity: SEL-0 earned exactly one gesture, so the shape's ARRIVAL is
// the gesture and a reader has nothing to switch on. A release, a second button, a
// wheel, a hover, a double press and a drag all remain unsayable here, which is
// what keeps "a provider gets input now" from meaning anything wider than the one
// sentence above.
//
// AND SINCE MSG-0 IT IS ALSO THE GESTURE THAT MOVES THE KEYBOARD. SEL-0 wrote here
// that a press made a pane the target of nothing else, and that sentence has one
// exception now and exactly one: Workshop remembers which external pane a maker
// last pressed into, and sends that pane the keys. It is still not capture --
// nothing is held across a release, the memory is a single kind resolved fresh at
// every spend, and a press ANYWHERE else takes it away again. See `PaneKey`.
//
// ---- THE SHAPES THAT ARE DELIBERATELY ABSENT --------------------------------
//
// No key RELEASE, no focus-changed notification, no capture, no hotkey
// registration, no wheel, no hover and no drag shape of any kind; no
// `PaneClosed`, `PaneUnavailable` or unload notification (Loom gives Workshop no
// participant-visible provider-unload event, and manufacturing one out of silence
// is the exact dishonesty WP-R0 was corrected for); no `PaneInstance`, because one
// `PaneRef` is one presentation; no `PaneConfig`, because no consumer has asked;
// no generic request/response envelope, because seven named shapes are seven
// readable sentences and an envelope is a framework.
//
// AND NO SHAPE IN WHICH A PROVIDER SAYS IT WANTS KEYS. Workshop does not ask and
// is not told: a press into a pane's room points the keyboard at it, and a
// provider that accepts neither key shape simply has the deliveries refused at
// Loom's gate, which is the substrate's own correct answer to being sent a shape
// it never declared. That is not an oversight -- it is the seam declining to grow
// a private copy of `zen.DescribeAccepted`, which is the door that already
// answers "does this target accept this shape" for every weave in the system.

#include "surface/vocabulary.hpp"

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// WORKSHOP ASKING THE ROOM WHO HAS PANES. Office-published, because Workshop
/// does not know any provider's role yet -- discovering that is the whole job.
///
/// It carries nothing. A field on it would be a filter, and a filter is a policy
/// about which providers may answer that nothing has asked for.
struct PaneCatalogRequested {
    ZEN_SHAPE(PaneCatalogRequested, 1);
};

/// A PROVIDER NAMING ONE PANE IT CAN PRESENT -- its key in that provider's own
/// namespace, and the two lines a maker reads in the picker.
///
/// WHOSE it is, is `mail.authored_role()`. See the header comment.
struct PaneOffered {
    std::string pane;    ///< the durable pane key, in the AUTHORING office's namespace
    std::string name;    ///< what the picker lists
    std::string summary; ///< one line, so a maker can tell what they are opening
    ZEN_SHAPE(PaneOffered, 1, ZEN_FIELD(pane), ZEN_FIELD(name), ZEN_FIELD(summary));
};

/// HOW MUCH PROSE WORKSHOP IS GRANTING THIS PANE, and the whole of what the
/// provider is told about where it is.
///
/// `rows` and `columns` are exactly the `surface::RegionFit` Workshop resolved
/// for the pane's BODY on the current screen with the current medium's text
/// metric. Sent when the pane opens, when a valid re-offer refreshes it, and
/// when that resolution changes -- and at no other time, so a resize that leaves
/// prose capacity equal says nothing.
struct PaneRoom {
    std::string pane;
    std::int64_t rows = 0;
    std::int64_t columns = 0;
    ZEN_SHAPE(PaneRoom, 1, ZEN_FIELD(pane), ZEN_FIELD(rows), ZEN_FIELD(columns));
};

/// WHAT THE PANE SAYS, inside the budget it was granted.
///
/// The rows are the provider's semantic material and Workshop copies them only
/// after they have passed the CURRENT room's bounds. Over-budget content is
/// refused whole rather than truncated: a pane presenting an unmarked partial
/// sentence as a provider's complete answer is the failure `detail::fit` exists
/// to prevent one row at a time, and truncation here would commit it silently.
struct PaneContent {
    std::string pane;
    std::vector<surface::SurfaceTextRow> rows;
    ZEN_SHAPE(PaneContent, 1, ZEN_FIELD(pane), ZEN_FIELD(rows));
};

/// A MAKER PRESSED INSIDE THE ROOM THIS PANE WAS GRANTED -- which pane, and where.
///
/// `row` AND `column` ARE THE `PaneRoom` LATTICE, AND NOTHING ELSE IS. Row 0 is the
/// first prose row of the BODY this provider was granted -- under Workshop's header
/// row, which the provider never received and is never told about -- and the pair
/// is inside `[0, rows) x [0, columns)` of the room currently in force. Workshop
/// resolves it through the same `external_body_place` that granted that room and
/// the same `prose_at` every bounded region in this application locates a press
/// with, so one measurer answers where a row is drawn and where a hand meets it.
/// No pixel, no cell, no canvas coordinate, no window origin, no pane rectangle,
/// no chrome geometry and no medium identity: a provider that learned any of those
/// could place itself on a screen it has no business seeing.
///
/// A PRESS THAT NAMES NO ROW IS NOT SENT. The header row, the padding below the
/// last prose row of a graphical medium, and anything outside the granted lattice
/// are all still consumed by the pane -- a pane that owns visible room owns
/// pointer refusal for that room -- and simply produce no sentence. Workshop does
/// not round them to a nearest row: a strip too short to fit prose is not a row,
/// and inventing one would hand a provider a press at a place it never wrote to.
///
/// WORKSHOP SENDS IT AND ASKS NOTHING BACK. No reply, no acknowledgement, no
/// disposition and no `consumed`: what a press MEANS is the provider's vocabulary
/// and Workshop holds none of it. If the answer is a changed presentation it
/// arrives as an ordinary `PaneContent`, judged against the same room as any other.
struct PanePressed {
    std::string pane;
    std::int64_t row = 0;
    std::int64_t column = 0;
    ZEN_SHAPE(PanePressed, 1, ZEN_FIELD(pane), ZEN_FIELD(row), ZEN_FIELD(column));
};

/// A KEY WENT DOWN WHILE THIS PANE HELD THE KEYBOARD (MSG-0).
///
/// ---- WHAT THE NUMBERS ARE ---------------------------------------------------
///
/// `scancode` is `zengine::input::scan`'s space and `modifiers` is
/// `zengine::input::mod`'s bitmask -- the SAME two numbers `input::KeyPressed`
/// carries, forwarded unchanged. That vocabulary is already this application's
/// normalized answer to "which key", written by the Input package against two
/// backends and documented there with each backend's honest reach; nothing here
/// re-derives it, translates it, or adds a value to it.
///
/// THE HEADER IS NOT INCLUDED AND THAT IS DELIBERATE. `PaneContent` carries
/// `SurfaceTextRow` VALUES, so it includes the type that defines them; these two
/// fields carry NUMBERS in a space somebody else owns, exactly as `PointerButton`
/// carries `space` and `SurfaceTextRow` carries `role`. A provider that wants the
/// constants by name includes `input/vocabulary.hpp` itself -- and a provider that
/// never asks for keys does not acquire a dependency on the input package because
/// this file grew two integers.
///
/// SO IT IS NOT A PLATFORM EVENT. No SDL event, no Windows virtual key, no
/// terminal escape sequence, no raw scancode off a keyboard, no key NAME (the
/// courtesy spelling a backend produced, which a provider switching on it would be
/// switching on the backend), no repeat flag, no timestamp, and no key release --
/// SEL-0's rule that a shape's ARRIVAL is the gesture, one gesture further on.
///
/// ---- WHAT HOLDING THE KEYBOARD MEANS, AND WHAT IT DOES NOT ------------------
///
/// It means a maker pressed inside this pane's visible room more recently than
/// they pressed anywhere else Workshop resolves a press. It is not a capture, not
/// a lease, not a lock and not a claim on the next key: Workshop resolves the
/// target fresh at every key from the panel list and the pane's granted room, so a
/// pane that closes, loses its provider or loses its room simply stops being the
/// answer, with nothing to release and no notification owed to anybody.
///
/// IT IS NOT EVERY KEY. The application keymap answers a small set of chorded
/// actions above every mode (which chords those are is the keymap's own truth --
/// `workshop/keymap.hpp` declares them and a maker's authored keymap can move
/// them; a comment here spelling them is how this sentence went stale twice), and
/// the Terminal overlay, pane management, the setup-name editor and the pane
/// picker each own the keyboard whole while they are open. A pane gets what is
/// left, which is every ordinary key -- including the printable ones Workshop
/// otherwise binds as commands, because a maker typing `p` into a field is typing
/// a `p`.
///
/// WORKSHOP SENDS IT AND ASKS NOTHING BACK, for `PanePressed`'s reason exactly:
/// there is no reply shape and no `consumed`. Whether the key meant anything is
/// the provider's business, and the only observable answer is an ordinary
/// `PaneContent`.
struct PaneKey {
    std::string pane;
    std::int64_t scancode = 0;  ///< `zengine::input::scan`'s space, unchanged
    std::int64_t modifiers = 0; ///< `zengine::input::mod`'s bitmask, unchanged
    ZEN_SHAPE(PaneKey, 1, ZEN_FIELD(pane), ZEN_FIELD(scancode), ZEN_FIELD(modifiers));
};

/// THE TEXT THE PLATFORM MADE OF A KEYSTROKE, while this pane held the keyboard.
///
/// `input::TextEntered`'s payload, forwarded unchanged: what the maker's own
/// keyboard layout committed, as UTF-8. It is the ONLY truthful route to a
/// character across this seam -- a provider that computed one from `PaneKey`'s
/// scancode would be re-deriving a layout it cannot see, which is the mistake
/// input/vocabulary.hpp exists to make unnecessary.
///
/// IT IS A SEPARATE SHAPE FROM `PaneKey` AND NOT A FIELD ON IT, because the two
/// facts have different populations: a key may produce no text and text may arrive
/// with no key. Both arrive for one keystroke that produces both, in that order,
/// exactly as they do one layer in.
///
/// AND IT IS NOT AN EDITOR. No selection, no clipboard, no composition, no IME and
/// no caret: what to do with a character is the provider's, and where the caret
/// then is, is something the provider draws in its own rows.
struct PaneTextInput {
    std::string pane;
    std::string text;
    ZEN_SHAPE(PaneTextInput, 1, ZEN_FIELD(pane), ZEN_FIELD(text));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP
