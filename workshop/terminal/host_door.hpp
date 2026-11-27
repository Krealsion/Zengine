// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_TERMINAL_HOST_DOOR_HPP
#define ZENGINE_WORKSHOP_TERMINAL_HOST_DOOR_HPP

// The host's Terminal door: a weave of the host's own, in `kTerminalDoorRole`, holding the
// terminal participant the host mounted. It authors a line through that participant, says what
// could be said next and hands one retained value over, each as an answer to Workshop's ask, and
// claims the participant's record as that role after each of its deliveries, each delivery the
// participant takes and each of the host's wakes. Host wiring: it is made with the participant,
// which no message can hand a weave.
// Workshop law: agents/workshop/terminal-door.md

#include "workshop/panes.hpp"
#include "workshop/terminal/complete.hpp"
#include "workshop/terminal/transcript.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"

#include "inventory/codec.hpp"

#include <zen/host/participants.hpp>
#include <zen/host/terminal_wiring.hpp>
#include <zen/switchboard.hpp>
#include <zen/terminal/input_lex.hpp>
#include <zen/terminal/session.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// Where a line can be addressed, made of Loom's participants claim as the door's delivery read
/// it (`loom::host::Participants`): every participant on the bus, its roles, the names of the
/// shapes it accepts and whether it is alive, `self` marked. A reading, not a registry: a sealed
/// candidate has no row, and a participant accepting any registered shape is shown by the shapes
/// it lists, as the bus lists them.
// WL-TERM-16 -- agents/workshop/terminal-door.md
inline std::vector<Destination> destinations_of(const loom::host::Participants& now,
                                                loom::WeaveId self) {
    std::vector<Destination> out;
    out.reserve(now.rows.size());
    for (const loom::host::Participant& row : now.rows) {
        Destination d;
        d.id = static_cast<std::uint64_t>(row.id);
        d.roles = row.roles;
        d.alive = row.alive;
        d.self = self.valid() && d.id == self.value;
        for (const loom::DeclaredShape& shape : row.accepts) {
            d.accepts.push_back(shape.name);
        }
        out.push_back(std::move(d));
    }
    return out;
}

/// `LineSlot` AS THE SEAM SPELLS IT.
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

/// THE DOOR. Mount it with `mount_terminal_door`, which gives it `terminal_door_grant()` and its
/// role, and claims the first picture.
class TerminalHostDoor final
    : public loom::WeaveBase<TerminalHostDoor, TerminalDoorState,
                             loom::Accept<TerminalLineAsked, TerminalCompletionAsked,
                                          TerminalValueAsked>,
                             loom::Emit<TerminalActed, TerminalCompletionOffered,
                                        TerminalValueAnswered>,
                             loom::Claims<TranscriptShown>> {
public:
    explicit TerminalHostDoor(loom::TerminalSession& participant) : me_(&participant) {}

    /// A door holding host wiring does not reload: a revived one would hold no participant.
    loom::LifecyclePolicy policy_config() const { return loom::LifecyclePolicy{0, true}; }

    /// THE ONE ACT THAT AUTHORS, `submit`: authored through the participant, then answered.
    // WL-TERM-02 -- agents/workshop/terminal-door.md
    void on(const TerminalLineAsked& asked, loom::Mail& mail) {
        if (!workshops(mail)) {
            (void)mail.answer(TerminalActed{false, kNotWorkshops});
            return;
        }
        submit_terminal_line(asked.line);
        ++state_.answered;
        (void)mail.answer(TerminalActed{true, std::string()});
    }

    /// What could be said next on a half-typed line: read, never authored (WL-TERM-05).
    // WL-TERM-16 -- agents/workshop/terminal-door.md
    void on(const TerminalCompletionAsked& asked, loom::Mail& mail) {
        TerminalCompletionOffered out;
        if (!workshops(mail)) {
            (void)mail.answer(std::move(out)); // a closed question, answered: nothing true to say
            return;
        }
        // `complete_line` takes the participant by const reference, and this runs on every
        // keystroke. Where a line can go is read off the bus only when the line is at its
        // address, as a value kept no longer than the answer.
        std::vector<Destination> reachable;
        std::optional<loom::host::Participants> now;
        if (read_command_line(asked.line).slot == LineSlot::Address) {
            now = loom::host::read_participants(mail);
        }
        const bool listing = now.has_value();
        if (listing) {
            reachable = destinations_of(*now, me_->id());
        }
        const Completion c = complete_line(*me_, asked.line, listing ? &reachable : nullptr,
                                           asked.required_first
                                               ? CompletionFieldOrder::RequiredFirst
                                               : CompletionFieldOrder::Declared);
        out.open = c.open;
        out.slot = slot_name(c.slot);
        out.partial = c.partial;
        out.heading = c.heading;
        out.candidates.reserve(c.candidates.size());
        for (const Candidate& in : c.candidates) {
            out.candidates.push_back(ShownCandidate{in.insert, in.display, in.detail});
        }
        ++state_.answered;
        (void)mail.answer(std::move(out));
    }

    /// ONE RETAINED VALUE, as content: the exact participant and observation Workshop authorized
    /// a gesture for, never a replay of the line or its authority (WL-TERM-17).
    // WL-TERM-17 -- agents/workshop/terminal-door.md
    void on(const TerminalValueAsked& asked, loom::Mail& mail) {
        const auto refuse = [&](std::string why) {
            (void)mail.answer(TerminalValueAnswered{false, std::move(why), {}, {}});
        };
        if (!workshops(mail)) {
            refuse(kNotWorkshops);
            return;
        }
        ++state_.answered;
        if (asked.participant <= 0 ||
            me_->id().value != static_cast<std::uint64_t>(asked.participant) ||
            asked.observation <= 0) {
            refuse("The source terminal is no longer available");
            return;
        }
        const loom::Transcript& record = me_->transcript();
        const auto observation = static_cast<std::uint64_t>(asked.observation);
        const auto value = record.retained_value(observation);
        if (!value) {
            refuse("This transcript value is no longer retained, or is only local text");
            return;
        }
        const auto entries = record.entries();
        const auto entry = std::find_if(entries.begin(), entries.end(),
                                        [&](const auto& e) { return e.seq == observation; });
        if (entry == entries.end()) {
            refuse("This transcript entry is no longer retained");
            return;
        }
        TerminalCaptureFacts facts;
        facts.participant = asked.participant;
        facts.observation = asked.observation;
        facts.kind = loom::name_of(entry->kind);
        if (entry->kind == loom::TranscriptKind::Submitted) {
            facts.addressing = loom::name_of(entry->addressing);
            if (entry->addressing == loom::Addressing::Weave) {
                facts.target = std::to_string(entry->target.value);
            }
            if (entry->addressing == loom::Addressing::Role) {
                facts.role = entry->role;
            }
        } else {
            facts.sender = std::to_string(entry->sender.value);
            facts.role = entry->authored_role;
            facts.authenticated_answer = entry->answers_ask;
        }
        try {
            const auto bytes = inventory::encode_pair(*value, {loom::to_value(facts)});
            if (bytes.size() > kMaxCarryBytes) {
                refuse("This value exceeds the 64 KiB carry limit");
                return;
            }
            (void)mail.answer(TerminalValueAnswered{true, {}, value->schema().name(),
                                                    loom::Bytes(bytes.begin(), bytes.end())});
        } catch (const std::exception& e) {
            refuse(std::string("This value could not be captured: ") + e.what());
        }
    }

    /// AFTER EACH OF ITS DELIVERIES the record is claimed again, when it moved: an act authored a
    /// line, and the picture Workshop reads next is the record that act left.
    // WL-TERM-02 -- agents/workshop/terminal-door.md
    void after_delivery(loom::Mail& mail) {
        TranscriptShown now = transcript_shown(me_);
        if (claimed_ && same_transcript(now, claimed_transcript_)) {
            return;
        }
        if (mail.as_role(kTerminalDoorRole).claim(now).accepted) {
            claimed_transcript_ = std::move(now);
            claimed_ = true;
        }
    }

    /// ...AND AT EACH OF THE HOST'S WAKES, from outside any delivery: an answer the participant
    /// heard moved the record with no delivery of this door's. The host's door names the
    /// claimant, which is this weave, as the role it holds.
    void claim_again(loom::Switchboard& bus) {
        TranscriptShown now = transcript_shown(me_);
        if (claimed_ && same_transcript(now, claimed_transcript_)) {
            return;
        }
        if (bus.role_claim_as(self_, kTerminalDoorRole, loom::to_value(now)).accepted) {
            claimed_transcript_ = std::move(now);
            claimed_ = true;
        }
    }

    /// The participant this door holds: the host's, which it mounted and ends.
    const loom::TerminalSession& participant() const noexcept { return *me_; }

private:
    /// The one asker this door answers in kind: Workshop, as its role.
    static bool workshops(loom::Mail& mail) { return mail.authored_from_role(kWorkshopProvider); }

    static constexpr const char* kNotWorkshops =
        "the Terminal's door answers Workshop's own asks, and this was not one";
    /// The largest pair a carry takes.
    static constexpr std::size_t kMaxCarryBytes = 65536;

    /// Compose and author one line through the participant, in Loom's own grammar: the one path
    /// that speaks as the terminal, stamped with its identity and judged against its grant.
    // WL-TERM-02 -- agents/workshop/terminal-door.md
    void submit_terminal_line(const std::string& line) {
        if (line.empty()) {
            return;
        }
        loom::TerminalSession& me = *me_;
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
        // THE WHOLE SENTENCE, at whatever length it takes to be a complete one. It is recorded on
        // the participant, unshortened, exactly as every other entry is: the pane wraps it across
        // as many of its own rows as it needs.
        me.record_notice("this pane speaks two verbs, and `ask` takes the same form as `send`: "
                         "send <addr> <Shape> <version> [args] -- an address is #12 for one "
                         "weave, @role for whoever holds a role, or * for everyone");
    }

    /// The versions a `Shape v<N>` can name. `parse_u64` answers in 64 bits and a schema version
    /// is 32, so a wider number is REFUSED rather than truncated -- `send @x Foo 4294967297` must
    /// not quietly become version 1.
    static constexpr std::uint64_t kMaxVersion = 0xFFFFFFFFull;

    loom::TerminalSession* me_;
    /// The last picture claimed, so a claim is made only when the record moved.
    TranscriptShown claimed_transcript_;
    bool claimed_ = false;
};

/// What the door may say: its three answers, to whoever asked, and the poke answers. It observes
/// Loom's participants claim, for where a line can go. Who may ask it is the asker's grant, which
/// the host gives Workshop alone (`allow_asking_terminal_door`).
inline loom::Grant terminal_door_grant() {
    loom::Grant g;
    g.allow_to_any(TerminalActed::zen_name, TerminalActed::zen_version);
    g.allow_to_any(TerminalCompletionOffered::zen_name, TerminalCompletionOffered::zen_version);
    g.allow_to_any(TerminalValueAnswered::zen_name, TerminalValueAnswered::zen_version);
    loom::host::allow_observe_participants(g);
    loom::allow_poke_answers(g);
    return g;
}

/// What the host's door is after its mount: its id, and the weave, which the host keeps to claim
/// the record again at its wakes. The bus owns the door.
struct MountedTerminalDoor {
    loom::WeaveId id{};
    TerminalHostDoor* door = nullptr;
};

/// THE PARTICIPANT HEARD SOMETHING: each delivery it takes may move its record (an answer, a
/// message, an ask's ending), with no delivery of the door's. For as long as this object lives
/// the host's tap has the door claim the record again after each one, outside the delivery, so
/// Workshop's next picture reads what it left. Declared after the bus, so it goes first.
class TerminalRecordWatch {
public:
    TerminalRecordWatch(loom::Switchboard& bus, loom::WeaveId participant, TerminalHostDoor& door)
        : bus_(bus) {
        tap_ = bus.add_observer([&bus, participant, &door](const loom::BusEvent& ev) {
            if ((ev.kind == loom::EventKind::Delivered ||
                 ev.kind == loom::EventKind::HandlerFailed) &&
                ev.target == participant) {
                door.claim_again(bus);
            }
        });
    }
    ~TerminalRecordWatch() { bus_.remove_observer(tap_); }
    TerminalRecordWatch(const TerminalRecordWatch&) = delete;
    TerminalRecordWatch& operator=(const TerminalRecordWatch&) = delete;

private:
    loom::Switchboard& bus_;
    loom::ObserverId tap_ = 0;
};

/// Mount the door over `terminal` in its role with its grant, and claim the first picture, so
/// Workshop's first reading of the record is the participant's.
inline MountedTerminalDoor mount_terminal_door(loom::Switchboard& bus,
                                               const loom::MountedTerminal& terminal) {
    auto weave = std::make_unique<TerminalHostDoor>(*terminal.session);
    TerminalHostDoor* raw = weave.get();
    const loom::WeaveId id =
        bus.register_weave(std::move(weave), terminal_door_grant(), std::string(kTerminalDoorRole));
    raw->zen_set_self(id);
    raw->claim_again(bus);
    return MountedTerminalDoor{id, raw};
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_TERMINAL_HOST_DOOR_HPP
