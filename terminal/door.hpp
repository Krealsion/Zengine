// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TERMINAL_DOOR_HPP
#define ZENGINE_TERMINAL_DOOR_HPP

// The door onto the terminal participant, composed onto Workshop's weave: the picture of its
// record said when it changed, the one act that authors, and the read that says what could be
// said next, all at the office Workshop holds. Compiled into Workshop; the pane's image includes
// none of it (WL-TERM-08).
// Workshop law: agents/workshop/terminal.md

#include "terminal/complete.hpp"
#include "terminal/transcript.hpp"

#include "workshop/panes.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"

#include <zen/switchboard.hpp>
#include <zen/terminal/input_lex.hpp>
#include <zen/terminal/session.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// The host's answer to `destinations`, read off `bus` at the call: every weave that is not a
/// sealed candidate, its office, accepted shapes and liveness, and `self` marked. A reading, not a
/// registry.
// WL-TERM-16 -- agents/workshop/terminal.md
inline std::vector<Destination> bus_destinations(const loom::Switchboard& bus, loom::WeaveId self) {
    std::vector<Destination> out;
    for (const loom::WeaveId id : bus.list_weaves()) {
        if (bus.sealed(id)) {
            continue; // a prepared candidate is outside the world: nothing can be sent to it
        }
        Destination d;
        d.id = id.value;
        d.office = bus.role_of(id);
        d.alive = bus.alive(id);
        d.self = self.valid() && id == self;
        for (const std::shared_ptr<const loom::Schema>& door : bus.accepted_schemas(id)) {
            if (door != nullptr) {
                d.accepts.push_back(door->name());
            }
        }
        out.push_back(std::move(d));
    }
    return out;
}

/// `LineSlot` AS THE SEAM SPELLS IT.
// WL-TERM-04 -- agents/workshop/terminal.md
inline const char* slot_name(LineSlot slot) noexcept {
    switch (slot) {
    case LineSlot::Verb: return kSlotVerb;
    case LineSlot::Address: return kSlotAddress;
    case LineSlot::Shape: return kSlotShape;
    case LineSlot::Version: return kSlotVersion;
    case LineSlot::Arguments: return kSlotArguments;
    }
    return kSlotVerb;
}

/// THE TERMINAL DOOR: the participant it reads is the host's (`Host::terminal`), and so is where
/// a line can be addressed (`Host::destinations`); what it keeps is only the last picture it said.
/// Workshop composes it over its `HostContext` (`TerminalDoorFacet`).
template <class Host>
class TerminalDoor {
public:
    using Accept = loom::Accept<TerminalActRequested, TerminalCompletionRequested>;
    using Emit = loom::Emit<TranscriptShown, TerminalActed, TerminalCompletionOffered>;

    explicit TerminalDoor(Host& host) : host_(&host) {}

    /// THE ONE ACT THAT AUTHORS, `submit`: authored through the participant, answered, then
    /// `self`, the weave composing this door, repaints its desk so the picture follows the answer.
    template <class Self>
    void on(Self& self, const TerminalActRequested& asked, loom::Mail& mail) {
        // AN OFFICE MAY ASK; ANONYMOUS SPEECH MAY NOT -- `on(DocumentActRequested)`'s rule, and
        // the reason it names nobody: a second presentation of this participant asks with no
        // edit here.
        if (mail.authored_role().empty()) {
            return;
        }
        if (asked.act != kTerminalSubmitAct) {
            (void)mail.answer(TerminalActed{false, "this door knows one act, and it is `submit`"});
            return;
        }
        if (host_->terminal == nullptr) {
            // Nothing to author through, and nowhere to record the attempt: the participant is the
            // transcript, so the sentence crosses as the refusal, beside the line it was typed on.
            (void)mail.answer(
                TerminalActed{false, "no terminal participant is mounted on this bus -- "
                                     "nothing was authored"});
            return;
        }
        submit_terminal_line(asked.line);
        (void)mail.answer(TerminalActed{true, std::string()});
        // THE ANSWER, THEN THE PICTURE. Submitting recorded at least one entry on the
        // participant, so the reading HAS changed and `say_transcript` will publish it on the
        // repaint this ends in -- which is how the pane learns what its own line came to
        // without this door telling it twice.
        self.repaint(mail);
    }

    /// What could be said next on a half-typed line: read, never authored.
    // WL-TERM-05 -- agents/workshop/terminal.md
    void on(const TerminalCompletionRequested& asked, loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            return;
        }
        TerminalCompletionOffered out;
        if (host_->terminal == nullptr) {
            // A CLOSED QUESTION, ANSWERED. `open` false is "there is nothing true to say here",
            // which is exactly the state a pane with no participant is in -- and answering it
            // is what keeps the asker from waiting on a door that will never reply.
            (void)mail.answer(std::move(out));
            return;
        }
        // And it authors nothing: `complete_line` takes the participant by const reference, and
        // this runs on every keystroke. Where a line can go is read off the bus only when the line
        // is at its address, as a value kept no longer than the answer.
        std::vector<Destination> reachable;
        const bool listing = host_->destinations &&
                             read_command_line(asked.line).slot == LineSlot::Address;
        if (listing) {
            reachable = host_->destinations();
        }
        const Completion c =
            complete_line(*host_->terminal, asked.line, listing ? &reachable : nullptr);
        out.open = c.open;
        out.slot = slot_name(c.slot);
        out.partial = c.partial;
        out.heading = c.heading;
        out.candidates.reserve(c.candidates.size());
        for (const Candidate& in : c.candidates) {
            out.candidates.push_back(ShownCandidate{in.insert, in.display, in.detail});
        }
        (void)mail.answer(std::move(out));
        // NO REPAINT. Nothing about this host changed: the participant was read and not
        // written, and what the answer becomes on a screen is the pane's own `PaneContent`,
        // which repaints on its own arrival like every other pane's.
    }

    /// WHAT THE PARTICIPANT'S RECORD HOLDS, TO WHOEVER IS PRESENTING IT -- derived on the
    /// repaint, compared against the last utterance, and said only when it changed.
    // WL-TERM-03 -- agents/workshop/terminal.md
    void say_transcript(loom::Mail& mail) {
        TranscriptShown now = transcript_shown(host_->terminal);
        if (transcript_said_ && same_transcript(now, said_transcript_)) {
            return; // no news is silence, and silence is what makes this seam terminate
        }
        said_transcript_ = now;
        transcript_said_ = true;
        // `to_any`, NOT ADDRESSED, AND SAID AS THE OFFICE -- `say_conditions`' own three
        // reasons, unchanged one shape over.
        (void)mail.as_role(kWorkshopProvider).publish(std::move(now));
    }

    /// A party has just arrived and has heard nothing: the next `say_transcript` says the record.
    void say_again() { transcript_said_ = false; }

private:
    /// Compose and author one line through the participant, in Loom's own grammar: the one path
    /// that speaks as the terminal, stamped with its identity and judged against its grant -- why
    /// the participant stayed this host's.
    // WL-TERM-02, WL-TERM-04 -- agents/workshop/terminal.md
    void submit_terminal_line(const std::string& line) {
        if (line.empty()) {
            return;
        }
        loom::TerminalSession& me = *host_->terminal;
        me.record_command(line);

        const std::vector<loom::Token> tok = loom::tokenize(line);
        // The verb table is the completer's too (complete.hpp): one list, so a third verb cannot
        // be learned by only one asker.
        const TerminalVerb* verb = tok.empty() ? nullptr : terminal_verb(tok[0].text);
        loom::Address to;
        std::uint64_t version = 0;
        if (verb != nullptr && tok.size() >= 4 && loom::parse_address(tok[1].text, to) &&
            loom::parse_u64(tok[3].text, version) && version <= kMaxVersion) {
            std::vector<loom::Arg> args;
            for (std::size_t i = 4; i < tok.size(); ++i) {
                args.push_back(loom::lex_arg(tok[i]));
            }
            const loom::TerminalResult r =
                verb->ask ? me.ask(to, tok[2].text, static_cast<std::uint32_t>(version), args)
                          : me.send(to, tok[2].text, static_cast<std::uint32_t>(version), args);
            if (!r) {
                // A local refusal, recorded as this participant's own notice, not an answer:
                // nothing was authored, and the core already words each outcome.
                me.record_notice(std::string(loom::name_of(r.outcome)) +
                                 (r.detail.empty() ? "" : ": " + r.detail));
            }
            return;
        }
        // THE WHOLE SENTENCE, at whatever length it takes to be a complete one. It is
        // recorded on the participant, unshortened, exactly as every other entry is: the pane
        // wraps it across as many of its own rows as it needs, so the length of this string is
        // a question about the GRAMMAR and never about the furniture.
        me.record_notice("this pane speaks two verbs, and `ask` takes the same form as `send`: "
                         "send <addr> <Shape> <version> [args] -- an address is #12 for one "
                         "weave, @office for whoever holds a role, or * for everyone");
    }

    /// The versions a `Shape v<N>` can name. `parse_u64` answers in 64 bits and
    /// a schema version is 32, so a wider number is REFUSED rather than
    /// truncated -- `send @x Foo 4294967297` must not quietly become version 1.
    static constexpr std::uint64_t kMaxVersion = 0xFFFFFFFFull;

    Host* host_;
    /// The last picture said, so the publication is silent when nothing changed.
    TranscriptShown said_transcript_;
    bool transcript_said_ = false;
};

} // namespace zengine::workshop

#endif // ZENGINE_TERMINAL_DOOR_HPP
