// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_TERMINAL_SEAM_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_TERMINAL_SEAM_VOCABULARY_HPP

// The terminal participant across the pane seam: the record the Terminal pane is shown, the one
// act it may ask for and the one read. The participant stays this host's (WL-TERM-02): what
// crosses is a picture of its record, said when it changed (WL-TERM-03), and two asks answered
// at the office this host holds. The completion is an ask, never a picture, because `compose`
// is a live fact about a conversation in progress (WL-TERM-05).

#include <zen/weave/shape.hpp>

#include <zen/value.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

// ---- The picture --------------------------------------------------------------

/// Which kind of entry, spelled on the wire rather than as an enumerator's number: a name
/// survives an image compiled at another time. `loom::TranscriptKind`'s kinds, in its words.
inline constexpr const char* kEntryCommand = "command"; ///< the maker typed it
inline constexpr const char* kEntryRefusal = "refusal"; ///< the participant refused it, locally
inline constexpr const char* kEntryNotice = "notice";   ///< a local statement of fact
inline constexpr const char* kEntrySubmitted = "submitted"; ///< authored onto the bus
inline constexpr const char* kEntryReceived = "received";   ///< arrived
inline constexpr const char* kEntryAnswer = "answer";       ///< ...and answers an ask

/// How a submitted entry was addressed, spelled for the same reason.
inline constexpr const char* kAddressWeave = "weave";     ///< `#12`
inline constexpr const char* kAddressRole = "role";       ///< `@office`
inline constexpr const char* kAddressPublish = "publish"; ///< `*`

/// One displayed observation. The instance plus observation identify an acquisition subject;
/// payloads remain at the participant. Capturing requires a fresh authorized gesture and an
/// exact owner read; these picture facts confer no authority or conversation ownership.
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
    std::int64_t observation = 0; ///< stable within the named terminal instance
    ZEN_SHAPE(ShownEntry, 2, ZEN_FIELD(kind), ZEN_FIELD(text), ZEN_FIELD(shape),
              ZEN_FIELD(version), ZEN_FIELD(addressing), ZEN_FIELD(target), ZEN_FIELD(role),
              ZEN_FIELD(recipients), ZEN_FIELD(sender), ZEN_FIELD(answers), ZEN_FIELD(observation));
};

/// What the terminal participant's record holds now, published to any (which weave presents it is
/// the load plan's business). `attached` false is a real reading -- no participant is mounted --
/// and `participant` then means nothing.
struct TranscriptShown {
    bool attached = false;
    std::int64_t participant = 0; ///< the identity the pane's header names
    std::vector<ShownEntry> entries; ///< the whole record, oldest first; at most 256
    std::int64_t dropped = 0;        ///< evicted from the record entirely -- gone, not scrolled
    ZEN_SHAPE(TranscriptShown, 2, ZEN_FIELD(attached), ZEN_FIELD(participant),
              ZEN_FIELD(entries), ZEN_FIELD(dropped));
};

// An exact historical value, acquired by a current attributed pane gesture (WL-TERM-17): the
// answer is an Inventory pair, never a replay or a live grant.
struct TerminalValueRequested {
    std::string pane;
    std::int64_t participant = 0, observation = 0, gesture = 0;
    ZEN_SHAPE(TerminalValueRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(participant),
              ZEN_FIELD(observation), ZEN_FIELD(gesture));
};
struct TerminalValueAnswered {
    bool available = false;
    std::string reason, label;
    loom::Bytes pair;
    ZEN_SHAPE(TerminalValueAnswered, 1, ZEN_FIELD(available), ZEN_FIELD(reason),
              ZEN_FIELD(label), ZEN_FIELD(pair));
};
struct TerminalCaptureFacts {
    std::int64_t participant = 0, observation = 0;
    std::string kind, addressing, target, role, sender;
    bool authenticated_answer = false;
    ZEN_SHAPE(TerminalCaptureFacts, 1, ZEN_FIELD(participant), ZEN_FIELD(observation),
              ZEN_FIELD(kind), ZEN_FIELD(addressing), ZEN_FIELD(target), ZEN_FIELD(role),
              ZEN_FIELD(sender), ZEN_FIELD(authenticated_answer));
};

// ---- The one act --------------------------------------------------------------

/// The one act: this seam authors a line and does nothing else. Mounting, detaching, clearing the
/// record and the vocabulary are the host's own boot decisions.
inline constexpr const char* kTerminalSubmitAct = "submit";

/// Ask the participant's holder to author one line, verbatim; Loom's own grammar reads it, and
/// the pane never parses it.
struct TerminalActRequested {
    std::string act;  ///< `kTerminalSubmitAct`
    std::string line; ///< the whole line, as typed
    ZEN_SHAPE(TerminalActRequested, 1, ZEN_FIELD(act), ZEN_FIELD(line));
};

/// What the act came to. `accepted` means recorded and run, not delivered. A local refusal arrives
/// on the participant's own record, as a new picture; `refusal` is only for no participant
/// mounted and an unknown act.
struct TerminalActed {
    bool accepted = false;
    std::string refusal; ///< empty exactly when accepted
    ZEN_SHAPE(TerminalActed, 1, ZEN_FIELD(accepted), ZEN_FIELD(refusal));
};

// ---- The one read -------------------------------------------------------------

/// What could be said next, given this line: asks the holder to run the completer; authors
/// nothing.
struct TerminalCompletionRequested {
    std::string line;
    ZEN_SHAPE(TerminalCompletionRequested, 1, ZEN_FIELD(line));
};

/// One thing the maker may say next: only what the list draws and what accepting writes.
struct ShownCandidate {
    std::string insert;  ///< what accepting this writes onto the line
    std::string display; ///< how the row reads
    std::string detail;  ///< what it MEANS; the first thing a narrow pane gives up
    ZEN_SHAPE(ShownCandidate, 1, ZEN_FIELD(insert), ZEN_FIELD(display), ZEN_FIELD(detail));
};

/// Which part of the line the maker is in, spelled. The pane compares two, for one rule: a
/// dismissal belongs to the part of the line it was made in.
inline constexpr const char* kSlotVerb = "verb";
inline constexpr const char* kSlotAddress = "address";
inline constexpr const char* kSlotShape = "shape";
inline constexpr const char* kSlotVersion = "version";
inline constexpr const char* kSlotArguments = "arguments";

/// Everything the participant's holder can say about the line. The selected candidate is the
/// pane's state and never crosses: a second owner of that cursor reset it on every recompute.
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
