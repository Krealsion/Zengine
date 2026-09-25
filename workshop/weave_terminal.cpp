// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The door onto the terminal participant: the picture said when it changed, the one act that
// authors, and the read that says what could be said next.
// Workshop law: agents/workshop/terminal.md (+2 registers; agents/workshop.md routes)

#include "weave.hpp"
#include "inventory/codec.hpp"

namespace zengine::workshop {

// WL-TERM-03 -- agents/workshop/terminal.md
void WorkshopWeave::say_transcript(loom::Mail& mail) {
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

// WL-TERM-02 -- agents/workshop/terminal.md
void WorkshopWeave::on(const TerminalActRequested& asked, loom::Mail& mail) {
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
    repaint(mail);
}

void WorkshopWeave::on(const TerminalValueRequested& asked, loom::Mail& mail) {
    const auto reason = authorize_pane_operation({asked.pane, kWorkshopProvider,
        TerminalValueRequested::zen_name, TerminalValueRequested::zen_version, asked.gesture}, mail);
    const auto refuse = [&](std::string why) {
        approved_operation_ = {};
        (void)mail.answer(TerminalValueAnswered{false, std::move(why), {}, {}});
    };
    if (!reason.empty()) { refuse(reason); return; }
    const auto* terminal = host_->terminal;
    if (!terminal || asked.participant <= 0 || terminal->id().value !=
            static_cast<std::uint64_t>(asked.participant) || asked.observation <= 0) {
        refuse("The source terminal is no longer available"); return;
    }
    const auto& record = terminal->transcript();
    const auto observation = static_cast<std::uint64_t>(asked.observation);
    const auto value = record.retained_value(observation);
    if (!value) { refuse("This transcript value is no longer retained, or is only local text"); return; }
    const auto entries = record.entries();
    const auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& e) { return e.seq == observation; });
    if (entry == entries.end()) { refuse("This transcript entry is no longer retained"); return; }
    TerminalCaptureFacts facts;
    facts.participant = asked.participant; facts.observation = asked.observation;
    facts.kind = loom::name_of(entry->kind);
    if (entry->kind == loom::TranscriptKind::Submitted) {
        facts.addressing = loom::name_of(entry->addressing);
        if (entry->addressing == loom::Addressing::Weave) facts.target = std::to_string(entry->target.value);
        if (entry->addressing == loom::Addressing::Role) facts.role = entry->role;
    } else {
        facts.sender = std::to_string(entry->sender.value);
        facts.role = entry->authored_role;
        facts.authenticated_answer = entry->answers_ask;
    }
    try {
        const auto bytes = inventory::encode_pair(*value, {loom::to_value(facts)});
        if (bytes.size() > 65536) { refuse("This value exceeds the 64 KiB carry limit"); return; }
        (void)mail.answer(TerminalValueAnswered{true, {}, value->schema().name(),
                                               loom::Bytes(bytes.begin(), bytes.end())});
    } catch (const std::exception& e) { refuse(std::string("This value could not be captured: ") + e.what()); }
}

// WL-TERM-05 -- agents/workshop/terminal.md
void WorkshopWeave::on(const TerminalCompletionRequested& asked, loom::Mail& mail) {
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
    // And it authors nothing: `complete_line` takes the participant by const reference, and this
    // runs on every keystroke. Where a line can go is read off the bus only when the line is at
    // its address, as a value kept no longer than the answer.
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

// WL-TERM-16 -- agents/workshop/terminal.md
std::vector<Destination> bus_destinations(const loom::Switchboard& bus, loom::WeaveId self) {
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

// WL-TERM-04 -- agents/workshop/terminal.md
const char* slot_name(LineSlot slot) noexcept {
    switch (slot) {
    case LineSlot::Verb: return kSlotVerb;
    case LineSlot::Address: return kSlotAddress;
    case LineSlot::Shape: return kSlotShape;
    case LineSlot::Version: return kSlotVersion;
    case LineSlot::Arguments: return kSlotArguments;
    }
    return kSlotVerb;
}

// WL-TERM-02, WL-TERM-04 -- agents/workshop/terminal.md
void WorkshopWeave::submit_terminal_line(const std::string& line) {
    if (line.empty()) {
        return;
    }
    loom::TerminalSession& me = *host_->terminal;
    me.record_command(line);

    const std::vector<loom::Token> tok = loom::tokenize(line);
    // The verb table is the completer's too (complete.hpp): one list, so a third verb cannot be
    // learned by only one asker.
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
            // A local refusal, recorded as this participant's own notice, not an answer: nothing
            // was authored, and the core already words each outcome.
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

// WL-KEY-01 -- agents/workshop/keyboard.md
void WorkshopWeave::command(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    // Each arm calls one operation. A maker's keymap row naming a retired id is kept byte for byte
    // and said at load (`kRetiredActions`); nothing here answers it.
    switch (session_.keymap.action_for(KeyContext::kCommand, k.scancode, k.modifiers)) {
    // The two setup gestures, ordinary commands: `s` writes the setup file and `r` reads it;
    // naming a layout is `layout.rename`'s.
    case Act::kSetupSave: save_setup(); break;
    case Act::kSetupRestore: restore_setup(mail); break;
    // The layout shelf: stepping is over the whole population, painted or not. The four that take
    // a position reach the keyboard only through the live layout: without a captured subject, a
    // keyboard can truthfully name only that one.
    case Act::kLayoutNext: step_layout(+1, mail); break;
    case Act::kLayoutPrevious: step_layout(-1, mail); break;
    case Act::kLayoutNew: new_layout(mail); break;
    case Act::kLayoutRemove: drop_layout(session_.setup.active_at, mail); break;
    case Act::kLayoutRename: open_layout_rename(session_.setup.active_at); break;
    case Act::kLayoutDuplicate:
        duplicate_layout(session_.setup.active_at, mail);
        break;
    case Act::kLayoutMoveLeft: shift_layout(session_.setup.active_at, -1); break;
    case Act::kLayoutMoveRight: shift_layout(session_.setup.active_at, +1); break;
    // Arrange the desk: a printable trigger pays the swallow rule and buys a mode whose own keys
    // need no modifier.
    case Act::kArrangeDesk: open_arrange_desk(); break;
    // WHAT CAN I DO WITH THIS? -- the contextual-action surface, on the subject
    // command mode can truthfully name.
    case Act::kContextOpen: open_context_ambient(); break;
    // Pane titles are a presentation preference with a key: one session bit, which the repaint
    // spends (`refresh_external_rooms` re-grants the rooms whose capacity moved). A toggle states
    // it, so it is written to the prefs file then, under three walls: no path is live-only, a
    // refused file is never overwritten (`prefs_bad_`), and a failed write says so.
    case Act::kPaneTitles: {
        load_prefs(); // a toggle before any surface exists still toggles the truth
        session_.pane_titles = !session_.pane_titles;
        std::string note =
            session_.pane_titles
                ? "pane titles shown"
                : "pane titles hidden -- a pane holding the keyboard still shows its own";
        bool bad = false;
        if (!host_->prefs_path.empty()) {
            if (prefs_bad_) {
                note += "; not saved: " + host_->prefs_path +
                        " could not be read and will not be overwritten";
                bad = true;
            } else {
                const Written wrote =
                    prefs_persist::save_file(host_->prefs_path, session_.pane_titles);
                if (!wrote.accepted) {
                    note += "; not saved: " + wrote.refusal;
                    bad = true;
                }
            }
        }
        say(note, bad);
        break;
    }
    case Act::kQuit: quit(mail); break;
    // EDIT CODE BOUND TO A KEY NAMES NO PANE, so it acts on none. Command mode's subject is the
    // selected object or the room (`open_context_ambient`), never a pane, and guessing one --
    // the selected pane, the keyboard's -- would be the pointing/selecting collapse WL-CTX-01
    // refuses. The key still does something truthful: it says where the gesture lives
    // (agents/workshop/code.md).
    case Act::kEditCode:
        say("edit code follows a pane you point at -- right-press the pane, then choose edit "
            "code",
            true);
        break;
    default: break;
    }
}

} // namespace zengine::workshop
