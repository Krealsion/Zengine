// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_TERMINAL_DOOR_FACET_HPP
#define ZENGINE_WORKSHOP_TERMINAL_DOOR_FACET_HPP

// The door onto the terminal participant, composed onto Workshop's weave: the picture of its
// record said when it changed, the one act that authors, and the read that says what could be
// said next, all at the office Workshop holds. Its bodies are in `weave_terminal.cpp`.
// Workshop law: agents/workshop/terminal.md

#include "terminal_seam_vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <string>

namespace zengine::workshop {

struct HostContext;
class WorkshopWeave;

/// THE TERMINAL DOOR: the participant it reads is the host's (`HostContext::terminal`), and so is
/// where a line can be addressed (`HostContext::destinations`); what it keeps is only the last
/// picture it said.
class TerminalDoorFacet {
public:
    using Accept = loom::Accept<TerminalActRequested, TerminalCompletionRequested>;
    using Emit = loom::Emit<TranscriptShown, TerminalActed, TerminalCompletionOffered>;

    explicit TerminalDoorFacet(HostContext& host) : host_(&host) {}

    /// The one act, `submit`: authored through the participant, answered, then the desk repainted
    /// so the picture follows the answer.
    void on(WorkshopWeave& self, const TerminalActRequested& asked, loom::Mail& mail);
    /// What could be said next on a half-typed line: read, never authored.
    void on(const TerminalCompletionRequested& asked, loom::Mail& mail);

    /// WHAT THE PARTICIPANT'S RECORD HOLDS, TO WHOEVER IS PRESENTING IT -- derived on the
    /// repaint, compared against the last utterance, and said only when it changed.
    void say_transcript(loom::Mail& mail);
    /// A party has just arrived and has heard nothing: the next `say_transcript` says the record.
    void say_again() { transcript_said_ = false; }

private:
    /// Compose and author one line through the participant, in Loom's own grammar: the one path
    /// that speaks as the terminal, stamped with its identity and judged against its grant -- why
    /// the participant stayed this host's (WL-TERM-02).
    void submit_terminal_line(const std::string& line);

    /// The versions a `Shape v<N>` can name. `parse_u64` answers in 64 bits and
    /// a schema version is 32, so a wider number is REFUSED rather than
    /// truncated -- `send @x Foo 4294967297` must not quietly become version 1.
    static constexpr std::uint64_t kMaxVersion = 0xFFFFFFFFull;

    HostContext* host_;
    /// The last picture said, so the publication is silent when nothing changed.
    TranscriptShown said_transcript_;
    bool transcript_said_ = false;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_TERMINAL_DOOR_FACET_HPP
