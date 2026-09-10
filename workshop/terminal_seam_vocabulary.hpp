// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_TERMINAL_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_TERMINAL_SEAM_VOCABULARY_HPP

// THE TERMINAL PARTICIPANT, ACROSS THE PANE SEAM -- what the Terminal pane is shown, and
// the two things it may ask for.
//
// ---- WHO HOLDS THE PARTICIPANT, AND WHY IT IS STILL THIS HOST -----------------
//
// ⭐ MEASURED, NOT PREFERRED: `loom::TerminalSession` CANNOT BE DRIVEN BY A MESSAGE. Its
// handler "sends nothing" by construction -- "not a reply, not a retry, not an
// acknowledgement" (`Loom/src/terminal/session.cpp`, `TerminalSession::handle`) -- and the
// only shapes it accepts at all are the three doors the host declared for it
// (`accepted_schemas()` returns `vocabulary_.doors()`: `Ack`, `Result`, `Refused`). There is
// no shape that makes it author a line and no shape by which it answers with its transcript.
// Reaching it AS A WEAVE would need a new sentence under `Loom/`, which this migration is
// fenced out of.
//
// So the participant stays exactly where its trust lives. It is a deliberately narrow,
// deliberately host-mounted identity: its grant is one rule -- it may say `SurfaceText` to
// whoever holds `zengine.skin`, and nothing else, ever (`workshop.cpp`) -- and Workshop
// holds a non-owning pointer to it (`HostContext::terminal`). A pane that could author as
// that identity would be a pane wearing the host's own narrow seat, which is the opposite of
// what the seat is for. `HostContext::terminal` therefore STAYS; what leaves the host is the
// PRESENTATION, which is the whole of the migration.
//
// ---- SO WHAT CROSSES IS A PICTURE, AND TWO ASKS -------------------------------
//
//     zengine.workshop   SHOWS  TranscriptShown                (published when it changed)
//     zengine.workshop   ACTS   TerminalActRequested        -> TerminalActed
//     zengine.workshop   READS  TerminalCompletionRequested -> TerminalCompletionOffered
//
// Answered at the office this host already holds, for `DocumentActRequested`'s reason: the
// party that holds the participant is the party that answers for it, and a second office
// would be a second answer to "who may speak as this terminal".
//
// ---- THE PICTURE IS THE WHOLE RECORD, AND THAT IS A MEASUREMENT ---------------
//
// ⚠ THE TRANSCRIPT IS BOUNDED BY ITS OWN OWNER at `loom::kTranscriptCapacity` = 256
// entries (`Loom/include/zen/terminal/transcript.hpp`). So this shape carries the WHOLE
// record rather than a window this host invented, and no number here is a guess about how
// tall a pane might be. What the built-in did with `entries_that_fit` -- decide how many
// entries this particular pane can show whole -- is a PRESENTATION act and moved with the
// presentation; so did the wrapping, which is why nothing here is a rendered line.
//
// AND `earlier` IS THE PANE'S ARITHMETIC NOW, not this host's. The built-in computed it as
// `record.size() - shown.size()`, which only the party that decided `shown` can do. The
// picture carries the record and the count that was evicted FOR GOOD; how many of the rest
// are above the top of the pane is a fact about the pane.
//
// ---- SAID WHEN IT CHANGES, ON `DocumentShown`'s OWN DISCIPLINE ----------------
//
// The transcript changes with no gesture into the pane at all -- an answer arrives from a
// weave the maker asked something of, a boot walk records a notice, a participant is mounted
// or is not. A pane that could only ask would show a record that is wrong most of the time.
// So the host says it: derived per repaint, compared against the last utterance, published
// only when the reading changed -- because a pane answers a publication by publishing its
// rows, and a repaint follows that, so an unconditional publication would not terminate.
//
// ---- ...AND THE COMPLETION IS AN ASK, WHICH IS ALSO A MEASUREMENT -------------
//
// ⚠ IT CANNOT BE A PICTURE. `complete_line` (`workshop/complete.hpp`) reads three things off
// the participant: `vocabulary().catalog()` and `describe()`, which are static for a
// session -- and `compose(shape, version, args)`, which is NOT. `compose` runs the real
// composition ladder over the arguments already finished and resolves `Ref` arguments
// against messages this participant has actually received, so its verdict is a live fact
// about a conversation in progress. A vocabulary picture would have carried the first two
// and silently lied about the third.
//
// So the pane asks, with the line it is holding, and hears what could be said next. This is
// the same door the maker's Return goes through, one step short of authoring: `compose` is
// const, every call on that path is const, and the ONLY path that authors is
// `TerminalActRequested`. Browsing candidates authors nothing -- the decision record's own
// sentence, now true because of which shape the pane sent rather than because of which
// method the painter called.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

// ---- The picture --------------------------------------------------------------

/// WHICH KIND OF ENTRY, spelled on the wire rather than sent as an enumerator's number --
/// `DocumentActRequested::act`'s rule and its reason: a number is a fact about one build's
/// ordering, and a name is what survives a shape read by an image compiled at another time.
/// These are `loom::TranscriptKind`'s five, in the participant's own vocabulary.
inline constexpr const char* kEntryCommand = "command"; ///< the maker typed it
inline constexpr const char* kEntryRefusal = "refusal"; ///< the participant refused it, locally
inline constexpr const char* kEntryNotice = "notice";   ///< a local statement of fact
inline constexpr const char* kEntrySubmitted = "submitted"; ///< authored onto the bus
inline constexpr const char* kEntryReceived = "received";   ///< arrived
inline constexpr const char* kEntryAnswer = "answer";       ///< ...and answers an ask

/// HOW A SUBMITTED ENTRY WAS ADDRESSED, spelled for `kEntry*`'s reason exactly.
inline constexpr const char* kAddressWeave = "weave";     ///< `#12`
inline constexpr const char* kAddressRole = "role";       ///< `@office`
inline constexpr const char* kAddressPublish = "publish"; ///< `*`

/// ONE ENTRY OF THE RECORD, AS ITS FACTS -- never as a rendered line.
///
/// ⚠ EVERY FIELD HERE IS ONE `terminal_line` READ, and there is not a sixth. The built-in's
/// renderer read exactly the kind, the text, the shape and version, the addressing and its
/// three targets, the sender and the answered ask; so that renderer could move to the pane
/// whole, and this shape is what it needs to keep saying the same sentences. Nothing else of
/// `loom::TranscriptEntry` crosses: the observation sequence, the lens, the correlation and
/// the retained message id are the participant's own bookkeeping, and a presentation that
/// learned them could start keeping a second conversation.
struct ShownEntry {
    std::string kind;          ///< one of the six `kEntry*` spellings
    std::string text;          ///< the three local kinds' prose; empty for a message entry
    std::string shape;         ///< the payload's shape name
    std::int64_t version = 0;  ///< ...and its version
    std::string addressing;    ///< `submitted` only: one of the three `kAddress*` spellings
    std::int64_t target = 0;   ///< `submitted` + `weave`: the addressed identity
    std::string role;          ///< `submitted` + `role`: the office
    std::int64_t recipients = 0; ///< `submitted` + `publish`: the fanout Loom returned
    std::int64_t sender = 0;   ///< `received`/`answer`: the BUS-STAMPED sender
    std::int64_t answers = 0;  ///< `answer`: the local ask number, or 0
    ZEN_SHAPE(ShownEntry, 1, ZEN_FIELD(kind), ZEN_FIELD(text), ZEN_FIELD(shape),
              ZEN_FIELD(version), ZEN_FIELD(addressing), ZEN_FIELD(target), ZEN_FIELD(role),
              ZEN_FIELD(recipients), ZEN_FIELD(sender), ZEN_FIELD(answers));
};

/// WHAT THE TERMINAL PARTICIPANT'S RECORD CURRENTLY HOLDS.
///
/// PUBLISHED `to_any` AND NOT ADDRESSED, for `StandingConditions`' and `DocumentShown`'s
/// reason: which weave presents this is the load plan's business, and a host that addressed
/// one would be a host with a pane compiled into it again.
///
/// ⚠ `attached` FALSE IS A REAL READING AND NOT AN ABSENCE. A host may mount no participant;
/// the built-in said so in its header and refused to author, and the pane says the same two
/// sentences from this one bool. `participant` is meaningless when it is false.
struct TranscriptShown {
    bool attached = false;
    std::int64_t participant = 0; ///< the identity the pane's header names
    std::vector<ShownEntry> entries; ///< the whole record, oldest first; at most 256
    std::int64_t dropped = 0;        ///< evicted from the record entirely -- gone, not scrolled
    ZEN_SHAPE(TranscriptShown, 1, ZEN_FIELD(attached), ZEN_FIELD(participant),
              ZEN_FIELD(entries), ZEN_FIELD(dropped));
};

// ---- The one act --------------------------------------------------------------

/// WHICH ACT. One, and the list is the design: this seam authors a line and does nothing
/// else. It cannot mount, cannot detach, cannot clear the record and cannot change the
/// participant's vocabulary -- all four of which are the HOST's own boot decisions, and none
/// of which a presentation has ever been able to make.
inline constexpr const char* kTerminalSubmitAct = "submit";

/// ASK THE PARTICIPANT'S HOLDER TO AUTHOR ONE LINE.
///
/// `line` IS WHAT THE MAKER TYPED, VERBATIM, and the grammar that reads it is Loom's own
/// (`loom::tokenize`, `loom::parse_address`, `loom::lex_arg`) -- the one command grammar,
/// never a second one. The pane holds the text and the caret; it does not parse the line to
/// decide what it means, and it could not: what a verb does is the participant's.
struct TerminalActRequested {
    std::string act;  ///< `kTerminalSubmitAct`
    std::string line; ///< the whole line, as typed
    ZEN_SHAPE(TerminalActRequested, 1, ZEN_FIELD(act), ZEN_FIELD(line));
};

/// WHAT THE ACT CAME TO.
///
/// ⚠ `accepted` MEANS THE LINE WAS RECORDED AND RUN, NOT THAT A MESSAGE WAS DELIVERED. The
/// participant records the command, composes, and submits -- and a SUBMITTED message's fate
/// is never told to its sender, which is the legend the pane prints under its own header.
/// A local refusal (an unknown shape, a bad address, a full ask book) is recorded by the
/// participant on its own transcript in the core's own words, so it arrives at the pane as a
/// new PICTURE rather than as this refusal: `refusal` is for the two things that stop the
/// line before the participant ever sees it -- no participant is mounted, and an act this
/// door does not know.
struct TerminalActed {
    bool accepted = false;
    std::string refusal; ///< empty exactly when accepted
    ZEN_SHAPE(TerminalActed, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal));
};

// ---- The one read -------------------------------------------------------------

/// WHAT COULD BE SAID NEXT, GIVEN THIS LINE.
///
/// The line is the PANE's -- it holds the text, the caret and the window onto it -- and this
/// asks the participant's holder to run the completer over it. It authors nothing.
struct TerminalCompletionRequested {
    std::string line;
    ZEN_SHAPE(TerminalCompletionRequested, 1, ZEN_FIELD(line));
};

/// ONE THING THE MAKER MAY SAY NEXT -- `Candidate`'s four presentation fields and no more.
///
/// `kind`, `shape`, `version` and `door` DO NOT CROSS. The built-in's `Candidate` carried
/// them and its painter read none of them: what the list draws is `display`, `detail` and
/// which row is chosen, and what an acceptance writes is `insert`. A field the presentation
/// cannot show is a field the presentation has no business holding.
struct ShownCandidate {
    std::string insert;  ///< what accepting this writes onto the line
    std::string display; ///< how the row reads
    std::string detail;  ///< what it MEANS; the first thing a narrow pane gives up
    ZEN_SHAPE(ShownCandidate, 1, ZEN_FIELD(insert), ZEN_FIELD(display), ZEN_FIELD(detail));
};

/// WHICH PART OF THE LINE THE MAKER IS IN -- `LineSlot`'s five, spelled.
///
/// It crosses because the pane needs it for one rule and only one: a maker's dismissal of
/// the list belongs to the part of the line it was made in, so moving on to the next word is
/// a new question and the list comes back for it. The pane compares two of these; it never
/// decides what one means.
inline constexpr const char* kSlotVerb = "verb";
inline constexpr const char* kSlotAddress = "address";
inline constexpr const char* kSlotShape = "shape";
inline constexpr const char* kSlotVersion = "version";
inline constexpr const char* kSlotArguments = "arguments";

/// EVERYTHING THE PARTICIPANT'S HOLDER CAN SAY ABOUT THE LINE.
///
/// `selected` IS NOT HERE AND WILL NOT BE. Which candidate a maker is standing on is the
/// PANE's state -- moved by its own keys, by a press on a row, and survived across a
/// recomputation on the pane's own rule (same slot and same partial, clamped). A `selected`
/// on the wire would be a second owner of a cursor, and the built-in already proved what
/// that costs: the arrow keys appeared to do nothing at all, because the recomputation that
/// followed each move reset it.
struct TerminalCompletionOffered {
    bool open = false;   ///< is there anything true to say at all?
    std::string slot;    ///< one of the five `kSlot*` spellings
    std::string partial; ///< the token being typed
    std::string heading; ///< what this list IS, and what it does not promise
    std::vector<ShownCandidate> candidates;
    ZEN_SHAPE(TerminalCompletionOffered, 1, ZEN_FIELD(open), ZEN_FIELD(slot),
              ZEN_FIELD(partial), ZEN_FIELD(heading), ZEN_FIELD(candidates));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_TERMINAL_SEAM_VOCABULARY_HPP
