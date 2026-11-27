// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_TERMINAL_TRANSCRIPT_HPP
#define ZENGINE_WORKSHOP_TERMINAL_TRANSCRIPT_HPP

// The host's derived reading of the terminal participant: the picture the Terminal pane is shown,
// and the comparison that decides whether it is worth saying. Workshop's terminal door reads it;
// the pane's image includes none of it (WL-TERM-08).
// Workshop law: agents/workshop/terminal-door.md

#include "workshop/terminal_seam_vocabulary.hpp"

#include <zen/terminal/session.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace zengine::workshop {

namespace detail {

/// `loom::TranscriptKind` AS THE SEAM SPELLS IT. One switch, and no default arm: a kind
/// added to the core arrives here as a compiler error rather than as a silent `notice`.
inline const char* entry_kind(loom::TranscriptKind kind) noexcept {
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
inline const char* entry_addressing(loom::Addressing addressing) noexcept {
    switch (addressing) {
    case loom::Addressing::Weave: return kAddressWeave;
    case loom::Addressing::Role: return kAddressRoleHolder;
    case loom::Addressing::Publish: return kAddressPublish;
    }
    return kAddressWeave;
}

} // namespace detail

/// THE PARTICIPANT'S RECORD, AS THE SEAM CARRIES IT: derived at the moment of the ask, holding
/// no `loom::TerminalSession`, no `Transcript` and no entry the renderer could not read.
// WL-TERM-02 -- agents/workshop/terminal-door.md
inline TranscriptShown transcript_shown(const loom::TerminalSession* me) {
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
        out.observation = static_cast<std::int64_t>(e.seq);
        out.kind = detail::entry_kind(e.kind);
        out.text = e.text;
        out.shape = e.shape;
        out.version = static_cast<std::int64_t>(e.version);
        out.addressing = detail::entry_addressing(e.addressing);
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

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_TERMINAL_TRANSCRIPT_HPP
