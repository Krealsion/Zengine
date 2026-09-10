// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE HOST'S DERIVED READING OF THE TERMINAL PARTICIPANT -- the picture the Terminal pane is
// shown, and the comparison that decides whether it is worth saying. Compiled once into
// `zengine-workshop-logic` and linked by the host and every suite.
//
// ⚠ WHAT USED TO BE IN THIS FILE. Every sentence the overlay drew -- the address forms, the
// six entry renderings, the legend, the wrapping, `entries_that_fit`, the omission marker,
// the completion list's rows and `paint_terminal` itself -- was PRESENTATION, and it moved to
// `terminal-pane/` whole. What is left is the two functions a picture needs: derive it, and
// say whether it changed.
// Workshop law: agents/workshop/terminal.md (+1 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

namespace {

/// `loom::TranscriptKind` AS THE SEAM SPELLS IT. One switch, and no default arm: a kind
/// added to the core arrives here as a compiler error rather than as a silent `notice`.
const char* entry_kind(loom::TranscriptKind kind) noexcept {
    switch (kind) {
    case loom::TranscriptKind::LocalCommand: return kEntryCommand;
    case loom::TranscriptKind::LocalRefusal: return kEntryRefusal;
    case loom::TranscriptKind::LocalNotice: return kEntryNotice;
    case loom::TranscriptKind::Submitted: return kEntrySubmitted;
    case loom::TranscriptKind::Received: return kEntryReceived;
    case loom::TranscriptKind::AnswerReceived: return kEntryAnswer;
    }
    return kEntryNotice;
}

/// ...and `loom::Addressing`, the same way.
const char* entry_addressing(loom::Addressing addressing) noexcept {
    switch (addressing) {
    case loom::Addressing::Weave: return kAddressWeave;
    case loom::Addressing::Role: return kAddressRole;
    case loom::Addressing::Publish: return kAddressPublish;
    }
    return kAddressWeave;
}

} // namespace

// WL-TERM-02, WL-TERM-03 -- agents/workshop/terminal.md
TranscriptShown transcript_shown(const loom::TerminalSession* me) {
    TranscriptShown shown;
    if (me == nullptr) {
        // NO PARTICIPANT IS A READING, NOT AN ABSENCE. The pane says "no participant was
        // mounted on this bus" from exactly this, and it is published like any other
        // reading -- so a host that mounts none still gets a Terminal pane that explains
        // itself rather than an empty rectangle.
        return shown;
    }
    shown.attached = true;
    shown.participant = static_cast<std::int64_t>(me->id().value);
    const loom::Transcript& record = me->transcript();
    // THE WHOLE RECORD, because its owner bounds it at `loom::kTranscriptCapacity` (256).
    // Deciding how many of these to show is the pane's, and so is how many rows each costs.
    const std::vector<loom::TranscriptEntry> entries = record.entries();
    shown.entries.reserve(entries.size());
    for (const loom::TranscriptEntry& e : entries) {
        ShownEntry out;
        out.kind = entry_kind(e.kind);
        out.text = e.text;
        out.shape = e.shape;
        out.version = static_cast<std::int64_t>(e.version);
        out.addressing = entry_addressing(e.addressing);
        out.target = static_cast<std::int64_t>(e.target.value);
        out.role = e.role;
        out.recipients = static_cast<std::int64_t>(e.recipients);
        out.sender = static_cast<std::int64_t>(e.sender.value);
        out.answers = static_cast<std::int64_t>(e.answers);
        shown.entries.push_back(std::move(out));
    }
    shown.dropped = static_cast<std::int64_t>(record.evicted());
    return shown;
}

// WL-TERM-03 -- agents/workshop/terminal.md
bool same_transcript(const TranscriptShown& a, const TranscriptShown& b) {
    if (a.attached != b.attached || a.participant != b.participant || a.dropped != b.dropped ||
        a.entries.size() != b.entries.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.entries.size(); ++i) {
        const ShownEntry& x = a.entries[i];
        const ShownEntry& y = b.entries[i];
        // FIELD BY FIELD, because a record is append-and-evict and two entries at the same
        // index after an eviction are two different entries. Comparing sizes alone would
        // have made a full transcript stop publishing at exactly the moment it started
        // dropping -- except that `dropped` moves with every eviction, which is why that
        // count is compared above rather than merely carried.
        if (x.kind != y.kind || x.text != y.text || x.shape != y.shape ||
            x.version != y.version || x.addressing != y.addressing || x.target != y.target ||
            x.role != y.role || x.recipients != y.recipients || x.sender != y.sender ||
            x.answers != y.answers) {
            return false;
        }
    }
    return true;
}

} // namespace zengine::workshop
