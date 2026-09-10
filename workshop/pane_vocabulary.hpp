// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP

// THE WHOLE PROTOCOL BETWEEN WORKSHOP AND A WEAVE THAT OFFERS IT A PANE, widened
// five times since. Sixteen shapes.
//
//     PaneCatalogRequested   Workshop  ->  everyone   "who has panes?"
//     PaneOffered            provider  ->  Workshop   "I have this one."
//     PaneActions            provider  ->  Workshop   "...and these are its actions, a key each."
//     PaneRoom               Workshop  ->  provider   "here is how much prose it gets."
//     PaneContent            provider  ->  Workshop   "here is what it says."
//     PaneCaret              provider  ->  Workshop   "...and here is where I am typing in it."
//     PanePressed            Workshop  ->  provider   "a maker pressed here, in that room."
//     PaneDragged            Workshop  ->  provider   "...and their hand is here now, still down."
//     PaneKey                Workshop  ->  provider   "a key went down, and you have the keyboard."
//     PaneTextInput          Workshop  ->  provider   "...and the platform made this text of it."
//     PaneWheel              Workshop  ->  provider   "the wheel turned over that room."
//     PaneActionRequested    Workshop  ->  provider   "a maker asked for this action of yours."
//     PaneRevealRequested    provider  ->  Workshop   "put my pane where the maker can see it."
//     PaneQuitRequested      Workshop  ->  everyone   "may this Workshop end?"
//     PaneQuitAnswered       provider  ->  Workshop   "yes" / "no, and here is what stands in the way."
//
// THE LAST FOUR ARE THE EDITOR'S MIGRATION, and each is a contract a built-in had and a pane
// could not say. A drag swept a selection across a document (`PaneDragged`); opening a
// source put the Editor on the desk and pointed the keys at it (`PaneRevealRequested`); and
// an orderly quit read the buffer's dirty state before it stopped the bus -- which it cannot
// read across a seam, so it ASKS (`PaneQuitRequested`), and every pane that accepts the
// question answers it (`PaneQuitAnswered`). None of the four names the Editor: a pane that
// wants a sweep, a reveal or a say in the exit accepts the shape, and one that does not is
// unchanged and never hears it.
//
// THE NINTH, TENTH AND ELEVENTH ARE ONE ACTION TRUTH REACHING ACROSS THE SEAM. Inside
// the host an action is a stable id, a label and a default gesture in one catalog; a
// binding is the gesture a maker's keymap file moved it to; execution stays with the
// dispatch site (workshop/keymap.hpp). A pane weave had none of that: its keys were in no
// legend and no maker's file could move them. `PaneActions` is the pane's rows of that
// same catalog -- id, label, default gesture -- sent beside its offer, judged whole under
// the office stamp exactly as the offer is, joined into the effective keymap under the
// same collision law the file meets, with the maker's authored overrides applied to
// them. `PaneActionRequested` is the dispatch site's half: when the keyboard pane holds
// the keys and the pressed gesture is one of ITS rows' effective bindings, the RESOLVED
// id crosses instead of the raw key, so an override reaches the pane and the pane never
// re-derives a binding it cannot see. Every other key still crosses as `PaneKey`.
//
// THE FIFTH IS THE FOURTH'S BUDGET READ BACKWARDS, which is what made it the one
// worth adding. `PaneRoom` grants a lattice of prose rows and columns; `PanePressed`
// names a place IN that lattice and says nothing else. Workshop already owns the
// geometry that answers WHICH pane a press landed on -- `bounds_of` -> `contains`,
// the same rectangle the painter used -- so the synchronous half of the question
// never crosses the wire and no `consumed` ever comes back. A press that
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
// THE EIGHTH IS THE FIFTH'S GESTURE WITHOUT THE PLACE. A press names a row
// because a row is what a press means; a wheel means "advance through what you are
// showing", and it crosses as the notches the wire already carries, forwarded
// unchanged -- `PaneKey`'s discipline for a pointer gesture. Which pane it reaches is
// the same topmost-occupancy answer a press spends, decided at the Workshop
// boundary, and it is sent only over a prose row of the granted body.
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
// restart. Nothing in this protocol claims any of those and nothing here may grow to.
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
// no gesture identity: the selection phase earned exactly one gesture, so the shape's ARRIVAL is
// the gesture and a reader has nothing to switch on. A release, a second button, a
// hover, a double press and a drag all remain unsayable here, which is what keeps
// "a provider gets input now" from meaning anything wider than the one sentence
// above -- the wheel is its own sentence (`PaneWheel`), not a field on this one.
//
// AND IT IS ALSO THE GESTURE THAT MOVES THE KEYBOARD. The selection phase wrote here
// that a press made a pane the target of nothing else, and that sentence has one
// exception now and exactly one: Workshop remembers which external pane a maker
// last pressed into, and sends that pane the keys. It is still not capture --
// nothing is held across a release, the memory is a single kind resolved fresh at
// every spend, and a press ANYWHERE else takes it away again. See `PaneKey`.
//
// ---- THE SHAPES THAT ARE DELIBERATELY ABSENT --------------------------------
//
// No key RELEASE, no pointer RELEASE, no focus-changed notification, no capture and no
// hover; no `consumed`, reply or disposition for any of the six inbound gestures; no
// `PaneClosed`, `PaneUnavailable` or unload notification (Loom gives Workshop no
// participant-visible provider-unload event, and manufacturing one out of silence is the
// exact dishonesty a research pass was corrected for); no `PaneInstance`, because one
// `PaneRef` is one presentation; no `PaneConfig`, because no consumer has asked; no
// generic request/response envelope, because sixteen named shapes are sixteen readable
// sentences and an envelope is a framework. A drag crosses (`PaneDragged`) and its END
// does not: every drag is a whole statement about where the hand is, so the one consumer
// needs no release, and a pane that would need one is the consumer that earns it.
//
// AND NO SHAPE IN WHICH A PROVIDER SAYS IT WANTS KEYS. Workshop does not ask and
// is not told: a press into a pane's room points the keyboard at it, and a
// provider that accepts neither key shape simply has the deliveries refused at
// Loom's gate, which is the substrate's own correct answer to being sent a shape
// it never declared. That is not an oversight -- it is the seam declining to grow
// a private copy of `zen.DescribeAccepted`, which is the door that already
// answers "does this target accept this shape" for every weave in the system.
// `PaneActions` IS NOT THAT SHAPE, and the sentence survives it: declaring an
// action is not declaring a want. A pane that declared rows holds the keys on the
// same terms as one that declared none -- by being pressed into, and never by
// asking -- and what its declaration changes is only which of the keystrokes it
// was going to receive anyway arrive as a resolved id rather than as a number.

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

/// A KEY WENT DOWN WHILE THIS PANE HELD THE KEYBOARD.
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
/// rule that a shape's ARRIVAL is the gesture, one gesture further on.
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

/// THE WHEEL TURNED WHILE THE POINTER WAS OVER THIS PANE'S BODY.
///
/// `dx` and `dy` are `input::PointerWheel`'s own notches, forwarded unchanged: +1.0
/// per notch AWAY from the maker (which every desktop reads as "up": earlier rows),
/// fractional exactly as a precise wheel reports. Workshop accumulates nothing and
/// converts nothing -- how many rows a notch is worth, and whether it is worth
/// anything at all, is the provider's grammar, exactly as a key's meaning is.
///
/// IT FOLLOWS THE POINTER AND NOT THE KEYBOARD. The pane under the wheel is the same
/// topmost-occupancy answer a press spends, so a pane a maker never pressed into is
/// scrolled by pointing at it, and a pane in front of it keeps the gesture for its
/// own cells. It is sent only while the pointer is over a prose row of the granted
/// body -- the header and the remainder under the last row send nothing,
/// `PanePressed`'s rule -- and only to a pane that holds a room.
///
/// NO PLACE. A press names a row because a row is what a press means; a wheel means
/// "advance through what you are showing", and a row on it would be Workshop
/// prescribing that a pane has one list under the pointer. A provider with two
/// lists spends it on whichever its own grammar says, which is the same answer it
/// gives Up and Down.
///
/// WORKSHOP SENDS IT AND ASKS NOTHING BACK, for `PaneKey`'s reason exactly. A pane
/// that does not accept the shape has the delivery refused at Loom's gate and is
/// unchanged; a pane that accepts it and has nothing to move is unchanged too.
struct PaneWheel {
    std::string pane;
    double dx = 0.0; ///< horizontal notches, +1.0 per notch to the right
    double dy = 0.0; ///< vertical notches, +1.0 per notch away from the maker
    ZEN_SHAPE(PaneWheel, 1, ZEN_FIELD(pane), ZEN_FIELD(dx), ZEN_FIELD(dy));
};

/// ONE ACTION A PANE CAN PERFORM, AS THE PANE DECLARES IT: a stable id in the pane's
/// own namespace, the label a legend prints beside its key, and the default gesture as
/// the SAME TWO NUMBERS `PaneKey` carries -- `zengine::input::scan`'s scancode and
/// `input::mod`'s bitmask, so a provider that declares rows and one that reads keys
/// name the same thing the same way.
///
/// `id` IS THE SPELLING A MAKER'S KEYMAP FILE NAMES (`files.up`), which makes it
/// durable in the way a pane key is: a pane that renames one has broken every authored
/// override that moved it. Workshop judges it at admission by its own law -- present,
/// bounded, printable ASCII with no space, unique within the shape, and never one of
/// Workshop's own action ids, which would let one authored row name two things -- and
/// never interprets it. A dotted `pane.verb` spelling is the convention, not a rule.
///
/// `scancode == input::scan::kUnknown` DECLARES A ROW WITH NO DEFAULT GESTURE, one a
/// maker may bind and nothing reaches by key until they do; `modifiers` is then `kNone`.
/// Any other scancode must be one the keymap file's grammar can name (`key_name_of`),
/// because an override is written in that grammar and admission refuses a default the
/// file could not spell back.
///
/// NO KEY NAME CROSSES, in either direction: a name is a spelling the host owns, and a
/// provider switching on one would be switching on the host's grammar.
/// THE HOST ACTIONS A PANE MAY DECLARE IT OWNS (`PaneActionRow::supersedes`), spelled in the
/// protocol because a pane naming one is a stranger to `workshop/keymap.hpp`. Workshop's own
/// catalog is what makes the id real, and a suite pins the two spellings against each other.
inline constexpr const char* kOwnableDocumentSave = "document.save";

struct PaneActionRow {
    std::string id;             ///< the durable action id, in the PANE's own namespace
    std::string label;          ///< what a legend prints beside the key
    std::int64_t scancode = 0;  ///< `zengine::input::scan`'s space; `kUnknown` = no default
    std::int64_t modifiers = 0; ///< `zengine::input::mod`'s bitmask
    /// ONE OF WORKSHOP'S OWN ACTION IDS THIS ROW STANDS IN FOR while this pane holds the
    /// keyboard, or empty -- the declaration that makes an operation the PANE'S here
    /// without naming the pane anywhere in the host (WL-KEY-15).
    ///
    /// A pane that holds a document of its own says `document.save`: while its keys are
    /// the maker's, the host's save is not requestable and the pane's own row is, and that
    /// is true of the two rows WHEREVER a maker's keymap file has moved either of them --
    /// supersession is by name, never by matching gestures. It buys the pane exactly one
    /// thing beside that: the collision law lets this row take the superseded row's key,
    /// because the two are one meaning with two scopes rather than two meanings on one
    /// gesture. An id Workshop does not declare supersedes nothing and is refused.
    ///
    /// The empty default is written out because it is load-bearing: five panes declare rows
    /// with four initializers and mean "stands in for nothing", and a member with no default
    /// would make each of those an incomplete aggregate this build refuses.
    std::string supersedes{};
    ZEN_SHAPE(PaneActionRow, 1, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(supersedes));
};

/// THE ACTIONS ONE PANE DECLARES -- sent by the provider beside its offer, and JUDGED
/// WHOLE under the office Loom stamped on it, exactly as the offer is.
///
/// ATOMIC BOTH WAYS. A shape that fails any law -- an empty office, a pane this office
/// never offered, a bad row, more than `kMaxPaneActionRows`, or a default that collides
/// with a binding active while a pane holds the keys -- joins nothing and leaves the
/// pane's previously admitted rows exactly as they were; a shape that passes REPLACES
/// them. What a pane declared is retained on its catalog row for as long as the row is,
/// so the maker's keymap file arriving later is applied to it then.
///
/// ROWS, NOT WANTS. Declaring rows does not point the keyboard at a pane, does not hold
/// it, and does not change which keys it receives; it changes how the keystrokes it was
/// going to receive are spelled. An empty `rows` is a legal declaration of nothing.
struct PaneActions {
    std::string pane;                ///< the pane key, in the AUTHORING office's namespace
    std::vector<PaneActionRow> rows; ///< at most `kMaxPaneActionRows` (workshop/keymap.hpp)
    ZEN_SHAPE(PaneActions, 1, ZEN_FIELD(pane), ZEN_FIELD(rows));
};

/// A MAKER PRESSED THE GESTURE ONE OF THIS PANE'S ROWS ANSWERS TO, while the pane held
/// the keyboard -- and this is the RESOLVED id, after the maker's own keymap moved the
/// row wherever they authored, so the pane acts on a name and never re-derives a binding
/// it cannot see.
///
/// SENT INSTEAD OF `PaneKey` FOR THAT KEYSTROKE, never beside it, and the character the
/// keystroke produced is swallowed exactly as Workshop's own printable triggers are, so
/// a pane's `r` row does not also type an `r` into its field. A keystroke matching no
/// row of the keyboard pane crosses as `PaneKey` and `PaneTextInput` unchanged: a `p`
/// typed into a field is still a `p`.
///
/// WORKSHOP SENDS IT AND ASKS NOTHING BACK, for `PaneKey`'s reason exactly.
struct PaneActionRequested {
    std::string pane;
    std::string id; ///< one of the ids this pane declared, as it declared it
    ZEN_SHAPE(PaneActionRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(id));
};

/// WHERE THIS PANE'S CARET IS, AND WHAT IT HAS SELECTED -- published BESIDE its rows by a
/// pane that has one, and by no other pane.
///
/// ---- WHY IT IS NOT A FIELD ON `PaneContent` ---------------------------------
///
/// ⭐ A CARET IS A FACT ABOUT A REGION AND MOST PANES HAVE NONE. `surface::SurfaceTextRegion`
/// has carried `caret_row`/`caret_col` and the four selection fields since it gained real
/// type, and every pane's rows are merged into one such region by Workshop -- but a pane
/// could not SAY them, so a migrated pane's editable line lost its insertion point (the
/// Files browser's authoring line, the Powers query, and Info's property draft each recorded
/// the same loss). The founder's rule, 2026-09-07: caret and selection are a PANE'S OWN
/// shape, published beside its rows, merged by Workshop into the region it assembles --
/// never a `PaneContent` version every pane pays for. So this is a second sentence a pane
/// MAY say, not a field every pane must carry: the four panes that have no caret send
/// nothing, their shape is unchanged, and their images do not recompile.
///
/// ---- THE LATTICE IS `PanePressed`'s, AND NOTHING ELSE IS ---------------------
///
/// `row` and `column` are the `PaneRoom` lattice -- row 0 is the first prose row of the
/// BODY this provider was granted, under Workshop's header row, which the provider never
/// receives and is never told about. Workshop adds its own header offset when it merges,
/// exactly as it subtracts one when it locates a press. No pixel, no cell, no canvas
/// coordinate and no region origin: a provider that learned any of those could place a
/// caret on a screen it has no business seeing.
///
/// ---- REFUSED WHOLE ----------------------------------------------------------
///
/// ⚠ JUDGED AGAINST THE CONTENT THIS PANE LAST HAD ACCEPTED, and refused ENTIRELY when it
/// names a row that content does not have -- caret and selection together, never one of
/// them. A caret on a row that is not there would be drawn at a place the maker is not
/// typing, which is worse than no caret at all; and a shape half-admitted would make
/// "where is the caret" have two answers. Refusal leaves the pane with NO caret rather
/// than with its previous one, for `PaneActions`' opposite reason: a stale caret is a
/// position, and a position that is wrong is read as a fact.
///
/// `row == surface::kNoCaret` IS THE PANE SAYING IT HAS NO CARET RIGHT NOW -- a legal,
/// ordinary sentence, and the one a pane says when its draft closes. It is not a refusal.
///
/// ---- WHAT IT IS NOT ---------------------------------------------------------
///
/// No blink, no shape, no width, no colour, no visibility flag, no scroll request, no
/// "make me visible", no second region, and no claim on the keyboard. A pane that publishes
/// a caret has not asked for anything; it has said where, inside the rows it already sent,
/// the insertion point of the text it already wrote is. Which medium draws it how is the
/// Skin's -- a bar between glyphs in a window, an inserted `_` in a cell projection --
/// exactly as it already is for every region this host composes.
struct PaneCaret {
    std::string pane;
    /// The prose row the caret is on, in the granted body lattice; `surface::kNoCaret`
    /// (-1) says this pane has no caret at the moment.
    std::int64_t row = -1;
    std::int64_t column = 0; ///< ...and the prose column it sits BEFORE
    /// THE SELECTION, THE SAME WAY AND IN THE SAME LATTICE. `surface::kNoSelection` (-1)
    /// on `sel_begin_row` says there is none; a caret with no selection is the ordinary
    /// case and costs four zeroes.
    std::int64_t sel_begin_row = -1;
    std::int64_t sel_begin_col = 0; ///< inclusive, a caret-like position
    std::int64_t sel_end_row = -1;  ///< reading-order end row
    std::int64_t sel_end_col = 0;   ///< exclusive, a caret-like position
    ZEN_SHAPE(PaneCaret, 1, ZEN_FIELD(pane), ZEN_FIELD(row), ZEN_FIELD(column),
              ZEN_FIELD(sel_begin_row), ZEN_FIELD(sel_begin_col), ZEN_FIELD(sel_end_row),
              ZEN_FIELD(sel_end_col));
};

/// THE MAKER'S HAND MOVED WHILE THE BUTTON WAS STILL DOWN, after a press that landed in a
/// row of this pane's room -- and this is where it is now, in the SAME lattice the press
/// named, deliberately unclamped.
///
/// ---- WHAT IT IS ---------------------------------------------------------------
///
/// `PanePressed`'s gesture, continued. Workshop remembers which pane took a press that named
/// a row (`Session::text_drag`, place `kExternalPane`) and, for every pointer motion until
/// the button comes up, resolves the position against THAT pane's current body -- the same
/// `external_body_place` and `prose_at` the press spent -- and sends it here. Physical
/// routing stays the host's: which pane, whether it still has a room, and where its body is
/// this instant are all Workshop's answers, and a pane in front of the dragged one takes
/// nothing, because a drag is the press's pane's until the hand lets go.
///
/// ---- WHY IT IS NOT CLAMPED --------------------------------------------------------
///
/// `row` and `column` may lie OUTSIDE `[0, rows) x [0, columns)`: a hand that has left the
/// body still means something to a document -- the built-in Editor stepped its caret one row
/// past the edge per motion and scrolled after it, which is how a selection is swept out of
/// the window -- and only the pane knows what past-the-edge means for what it is showing.
/// A row that is a clamped guess would hand the pane a position it never wrote to; a signed
/// offset says exactly where the hand is and lets the pane decide. The press is different:
/// a press outside the body is not sent at all, because a press names a row and a row that
/// is not there is not a row (`PanePressed`).
///
/// ---- WHEN IT STOPS ------------------------------------------------------------------
///
/// On the button-1 release, with nothing sent: every drag is a complete sentence ("extend
/// to here"), so a pane needs no ending to act on the last one it heard. When the pane
/// loses its room (the desk closed it, a shrink lost its slot), the record is dropped and
/// nothing more is sent. When the pane's body CHANGES under the hand (a re-grant), the next
/// motion is resolved against the new body -- the hand did not move, the room did, and the
/// pane's own rule decides what that row means now. A pane that kept drag state of its own
/// would need to hear the release; the one consumer keeps none, and the release shape is
/// named as absent rather than added for nobody.
///
/// WORKSHOP SENDS IT AND ASKS NOTHING BACK, for `PanePressed`'s reason exactly. A pane that
/// does not accept the shape has every motion refused at Loom's gate and is unchanged,
/// which is `PaneWheel`'s posture for a pane with nothing to scroll.
struct PaneDragged {
    std::string pane;
    std::int64_t row = 0;    ///< a prose row of the granted BODY; may be < 0 or >= rows
    std::int64_t column = 0; ///< a prose column of the same region; may be < 0 or >= columns
    ZEN_SHAPE(PaneDragged, 1, ZEN_FIELD(pane), ZEN_FIELD(row), ZEN_FIELD(column));
};

/// PUT THIS PANE WHERE THE MAKER CAN SEE IT, AND POINT THE KEYS AT IT -- said by the pane's
/// own office, about a pane it offered, at the end of an act a maker asked for.
///
/// ---- WHAT WORKSHOP DOES WITH IT ------------------------------------------------------
///
/// Exactly what it did for the built-in Editor when a source was opened: seat the pane on
/// the active desk if it is not there (through `add_pane`, the picker's own membership door,
/// with the picker's own trial seat and the picker's own refusal when the screen has no slot
/// for it), select it, and point the keyboard at it. A reveal is a statement about the DESK
/// -- membership, selection, keys -- and touches no file, no provider and no other pane's
/// rows.
///
/// ---- WHY A PANE MAY SAY IT AT ALL ----------------------------------------------------
///
/// `PaneOffered` puts a pane in the LIST and never on the screen, and that stays true: this
/// shape is refused for a pane the office never offered, and it is honoured through the
/// ordinary door a maker's own picker press goes through. What it buys is the half of "open
/// a source" that lived in the host while the host held the document: a Files row pressed
/// by a maker asks the Editor to open; the Editor installs the document and asks to be
/// shown. Without it a source opened from Files would be open in a weave nobody could see.
/// The abuse is named rather than assumed away: an office that asked for this on every
/// beat would keep pulling its pane in front and taking the keys, and Workshop says on the
/// notice line which pane asked, every time, so the maker can read who did it and remove
/// the pane. Nothing here stops that office; a bound on how often is a later seam.
///
/// ---- IT IS ASKED, AND ANSWERED ------------------------------------------------------
///
/// A pane learns the outcome (`PaneRevealAnswered`), because an acquisition that ends in a
/// presentation is ONE transaction and the pane is the party that has to leave itself
/// consistent with it: the Editor reads a source, asks to be shown, and installs the
/// document only if the answer is yes -- so a screen with no room leaves the pane's prior
/// document, its caret and its history exactly as they were, and the refusal reaches
/// whoever asked for the source. A pane that wants none of that may ignore the answer; the
/// ask costs it nothing.
///
/// ---- WHAT IT IS NOT ------------------------------------------------------------------
///
/// Not an offer (the pane must already be in the catalog), not a room grant (the room
/// follows the seat on the next repaint, as it always has), and not focus authority (the
/// keys are pointed the way a press points them, and the next press elsewhere takes them
/// away). If the screen has no slot, Workshop refuses in the picker's own words, says so on
/// its own notice line, and the pane is exactly as it was.
struct PaneRevealRequested {
    std::string pane;
    ZEN_SHAPE(PaneRevealRequested, 1, ZEN_FIELD(pane));
};

/// WHAT THE DESK DID WITH A REVEAL -- Workshop's answer to `PaneRevealRequested`, on the
/// delivery that asked.
///
/// `revealed` is a fact about the desk at the instant of the answer: the pane is seated on
/// the active setup, selected, and holding the keys if its kind takes them. A refusal
/// carries the picker's own sentence, the one a maker reads on the notice line, so a pane
/// that wants to say it in its own rows can say the same words rather than invent a second
/// vocabulary for the same event.
///
/// ⚠ IT IS AN ANSWER, NOT A PROMISE. The desk can move again on the next gesture -- a maker
/// removes the pane, another reveal pushes past it, an arrangement takes its slot -- and
/// nothing here is voided when it does. A pane spends this answer to decide what it does
/// with the act it was in the middle of, and then reads the room the way every pane does.
struct PaneRevealAnswered {
    std::string pane;
    bool revealed = false;
    std::string refusal;
    ZEN_SHAPE(PaneRevealAnswered, 1, ZEN_FIELD(pane), ZEN_FIELD(revealed), ZEN_FIELD(refusal));
};

/// MAY THIS WORKSHOP END? -- asked of everyone that can hold a maker's unsaved work, before
/// an orderly quit stops the bus.
///
/// ---- WHY IT IS A PUBLICATION, AND WHY THE COUNT IS THE INSTRUMENT ---------------------
///
/// The built-in Editor's dirty buffer was session state, so `quit()` read it and refused
/// synchronously. A document that lives in a weave cannot be read; it can be asked. And a
/// host must not name the one pane it thinks holds work -- that would be the Editor compiled
/// back into it by address -- so it asks the ROOM: an office-authored publication, whose
/// fan-out count Loom hands back is exactly how many parties accept the question. Zero
/// accepters is an authoritative "nobody holds anything": the quit proceeds. N accepters is
/// N answers Workshop waits for, and it quits only when every one permitted it.
///
/// ---- THE OBLIGATION ------------------------------------------------------------------
///
/// A pane that accepts this shape MUST answer it (`mail.answer`, with `PaneQuitAnswered`),
/// promptly and truthfully, on the delivery that asked: an accepter that stays silent holds
/// the quit open, and Workshop says so on the notice line rather than guessing. "Permitted"
/// means the pane holds nothing an orderly end would lose; a refusal names what stands in
/// the way and what the maker can do about it, in the pane's own words. A PENDING operation
/// whose answer could still change the answer -- a paste in flight -- is a refusal ("quit
/// again"), never a permission: the question is about the instant of the answer, and an
/// answer that a queued message could falsify is not an answer.
///
/// ---- WHAT WORKSHOP DOES BETWEEN THE ASK AND THE LAST ANSWER --------------------------
///
/// It holds every input gesture it receives -- key, text, press, motion, wheel -- rather
/// than routing it, so no keystroke can reach a pane after that pane answered "clean" and
/// before the bus stops. On a refusal the held gestures are replayed in order, so a maker
/// who typed through a refused quit loses nothing; on a permission they are dropped, which
/// is what a process that has ended does with keys typed after it. The whole exchange is
/// one drain of the bus; a maker sees the refusal or the exit, and not the wait.
struct PaneQuitRequested {
    ZEN_SHAPE(PaneQuitRequested, 1);
};

/// ONE PANE'S ANSWER TO `PaneQuitRequested`. `permitted` with an empty `refusal`, or the
/// pane's own sentence naming what an orderly end would lose and how to settle it. `pane` is
/// the key the answering office offered, so a refusal reads as somebody's; the office itself
/// is not on the wire (an answer is personal speech) and is not needed -- the sentence is
/// written to stand alone.
struct PaneQuitAnswered {
    std::string pane;
    bool permitted = false;
    std::string refusal;
    ZEN_SHAPE(PaneQuitAnswered, 1, ZEN_FIELD(pane), ZEN_FIELD(permitted), ZEN_FIELD(refusal));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_VOCABULARY_HPP
