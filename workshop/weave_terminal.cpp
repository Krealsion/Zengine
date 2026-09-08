// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE DOOR ONTO THE TERMINAL PARTICIPANT -- the picture said when it changed, the one act
// that authors, and the read that says what could be said next. Compiled once into
// `zengine-workshop-logic` and linked by the host and every suite.
//
// ⚠ WHAT USED TO BE IN THIS FILE. `toggle_terminal`, `terminal_key`, `terminal_press`,
// `completion_selectable`, `move_completion`, `accept_completion` and `refresh_terminal` were
// the overlay: a mode with its own keyboard context, its own press chain and its own
// snapshot. All seven are gone. What a maker does to the terminal is now what a maker does to
// any pane -- press into it, and its own keys are its own rows -- and the one thing this host
// still does is hold the participant and answer for it.
//
// `submit_terminal_line` SURVIVED, ALMOST UNCHANGED, and that is the measurement this
// migration was for: the part of the terminal that could not move is the part that speaks as
// a Loom identity, and it is about forty lines.
// Workshop law: agents/workshop/terminal.md (+2 registers; agents/workshop.md routes)

#include "weave.hpp"

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
        // Nothing to author through, and nowhere to record the attempt: the participant IS
        // the transcript. The built-in said this on the tool's own notice line, where it
        // was visible the moment the pane closed; a pane that cannot be closed needs the
        // sentence beside the line it was typed on, so it crosses as the refusal.
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
    // AND IT AUTHORS NOTHING. `complete_line` takes the participant by const reference;
    // every method it reaches (`vocabulary()`, `describe()`, `compose()`) is const, and the
    // only path that authors goes through the participant's own channel, which a const
    // reference cannot touch. This is the call that runs on every keystroke, and it is the
    // one that must never send.
    const Completion c = complete_line(*host_->terminal, asked.line);
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
    // THE VERB TABLE IS THE COMPLETER'S TOO (complete.hpp). It used to be
    // two string literals in the condition below, which was one answer while
    // one thing asked the question; a list a maker can be SHOWN is a second
    // asker, and two lists of two verbs is how the third verb gets learned by
    // only one of them.
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
            // A LOCAL refusal, and it is recorded as this participant's own
            // notice rather than dressed up as an answer: nothing was
            // authored, so nothing was denied by anybody. The core already
            // words each outcome; repeating it here in different words would
            // be a second vocabulary for one fact.
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

// WL-KEY-01 -- agents/workshop/keyboard.md; WL-PANE-12 -- agents/workshop/panes-and-windows.md
void WorkshopWeave::command(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    // EVERY ARM CALLS THE OPERATION IT ALWAYS CALLED; what the keymap changed is only
    // how a gesture becomes an action. Exact matching split the old `shift ?` pairs
    // into declared siblings (`object.left`/`object.narrower`,
    // `builder.build`/`builder.build-realize`) -- one gesture family spelled two ways
    // remains the design (hjkl's own decision), each half its own remappable row now,
    // and the accidental subset aliases the old per-site tests produced (Ctrl+N
    // created; Alt+Q quit) are gone on purpose.
    switch (session_.keymap.action_for(KeyContext::kCommand, k.scancode, k.modifiers)) {
    case Act::kObjectNext: select_next(); break;

    case Act::kObjectNew: create_object(); break;
    case Act::kObjectDelete: delete_object(); break;
    case Act::kObjectLeft: move_by(-1, 0); break;
    case Act::kObjectDown: move_by(0, +1); break;
    case Act::kObjectUp: move_by(0, -1); break;
    case Act::kObjectRight: move_by(+1, 0); break;
    case Act::kObjectNarrower: size_by(-1, 0); break;
    case Act::kObjectTaller: size_by(0, +1); break;
    case Act::kObjectShorter: size_by(0, -1); break;
    case Act::kObjectWider: size_by(+1, 0); break;
    case Act::kWorkspaceNarrower: resize_workspace(-4); break;
    case Act::kWorkspaceWider: resize_workspace(+4); break;
    case Act::kPicker: open_picker(); break;
    // ⭐ THE NINE BUILDER ARMS ARE GONE FROM COMMAND MODE (VD-22). `b`, `B`, `P`, `R`, `o`,
    // `c`, `C`, `f` and `e` were dispatched from here, each of them opening with "with no
    // Builder panel open this is an unbound key" -- which is what a command-mode row acting
    // on one pane's subject always has to say. The Builder is a weave now and hears its own
    // ids while it holds the keyboard, so the answer to "what does `b` do from here" is that
    // there is no `b` here to ask about.
    //
    // ⭐ AND THE TERMINAL'S GLOBAL CHORD WENT THE SAME WAY (VD-22, VD-24). `workshop.terminal`
    // was a GLOBAL row -- the last one that opened one particular pane from anywhere -- and a
    // pane a maker opens from the picker needs no key of its own, exactly as Attention's
    // `Ctrl+a` needed none once the current-condition view became a pane.
    // The editor's deliberate discard, reachable from command mode too so the quit
    // refusal names a gesture that works where the maker is standing.
    case Act::kEditorDiscard: discard_source_edits(); break;
    // THE TWO SETUP GESTURES: ordinary maker commands beside `+ panel`,
    // deliberately not another `^`-pair beside the document's. they are
    // both FILE operations and nothing else -- `s` writes, `r` reads, and naming a
    // layout is `layout.rename`'s.
    case Act::kSetupSave: save_setup(); break;
    case Act::kSetupRestore: restore_setup(mail); break;
    // THE LAYOUT SHELF: four ordinary command-mode gestures over the run of
    // desk arrangements this Workshop is holding. Stepping is over the WHOLE
    // population, painted or not, which is what keeps the band's derived tab window a
    // presentation rather than a bound on what a maker can reach.
    //
    // THE FOUR THAT TAKE A POSITION REACH THE KEYBOARD ONLY THROUGH THE LIVE LAYOUT,
    // and that is deliberate: `^w` closes the layout a maker is standing on, and the
    // rest are the contextual menu's, on the tab a maker pointed at. A keyboard with
    // no captured subject can truthfully name one layout, which is the live one --
    // `open_context_ambient`'s own rule about panes, one surface over.
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
    // ARRANGE THE DESK (a mode, rescoped to the desk): a printable trigger pays
    // the swallow rule -- armed centrally from the binding -- and buys a
    // mode whose own keys need no modifier at all (P48).
    case Act::kArrangeDesk: open_arrange_desk(); break;
    // WHAT CAN I DO WITH THIS? -- the contextual-action surface, on the subject
    // command mode can truthfully name.
    case Act::kContextOpen: open_context_ambient(); break;
    // PANE TITLES ARE A PRESENTATION PREFERENCE WITH A KEY. The flip is one
    // session bit; everything it changes on screen -- the arrangeable panes' header
    // rows, the row returned to or taken back from each provider's budget -- follows
    // from the ordinary repaint this keystroke already earns (`refresh_external_rooms`
    // re-grants exactly the rooms whose capacity moved). The notice says which state
    // the toggle landed in, because a maker with no external pane open would otherwise
    // watch nothing change; its second half names the one exception, which is the
    // keyboard-identity law, not a courtesy.
    //
    // AND THE PREFERENCE IS DURABLE: a toggle is the maker STATING it, so
    // this is the moment it is written -- to the prefs file, whose ordinary home is
    // the per-user configuration root, so the choice follows the maker across launch
    // directories rather than living exactly as long as the run. Three quiet walls:
    // no path chosen means live-only (which is `--isolated`'s promise); a prefs file
    // that stands refused is never overwritten (`prefs_bad_` -- the do-not-rewrite
    // law); and a failed write says so on the same notice, because a preference a
    // maker believes saved and is not is the quiet wrong answer.
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
    case Act::kQuit: quit(); break;
    default: break;
    }
}

} // namespace zengine::workshop
