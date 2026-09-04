// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s sections -- the terminal overlay -- compiled once into
// `zengine-workshop-logic` and linked by the host and every suite; the declarations, the
// constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/terminal.md (+6 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- The terminal overlay ------------------------------------------------

// WL-TERM-01 -- agents/workshop/terminal.md
void WorkshopWeave::toggle_terminal() {
    session_.terminal.open = !session_.terminal.open;
    if (!session_.terminal.open) {
        // Said on the way OUT and not on the way in, because the notice line
        // is not painted while the pane covers it -- an "opened" notice would
        // be a sentence nobody could read that then reappeared, stale, at the
        // moment it stopped being true. The pane's own header says how to
        // close it, which is the fact a maker needs while it is open.
        say("terminal closed -- " + hotkey(Act::kTerminalToggle) + " reopens it", false);
    }
    refresh_terminal();
}

// WL-TERM-01, WL-TERM-05 -- agents/workshop/terminal.md
// WL-TEXT-02, WL-TEXT-04 -- agents/workshop/text-box.md
void WorkshopWeave::terminal_key(const zengine::input::KeyPressed& k) {
    TerminalPane& pane = session_.terminal;
    // THE LINE'S OWN VOCABULARY FIRST: the six editing keys this switch used
    // to spell, and selection, clipboard, word movement and history behind them, all
    // through the one component call every consumer now makes. A consumed gesture
    // still reaches `refresh_terminal`, for the reason the caret keys always fell
    // through rather than `return`ing the way Up/Down do: an edit or a caret move
    // changes whether the caret is AT THE END, which is the question the completer is
    // allowed to be asked.
    if (pane.input.consume(k.scancode, k.modifiers, session_.clipboard)) {
        refresh_terminal();
        return;
    }
    switch (session_.keymap.action_for(KeyContext::kTerminal, k.scancode, k.modifiers)) {
    case Act::kTerminalSubmit: submit_terminal_line(); break;
    case Act::kTerminalBack:
        if (completion_selectable()) {
            // THE LIST GOES AWAY AND THE LINE IS UNTOUCHED. A maker who wanted
            // the line gone presses it again; a maker who wanted only the list
            // gone has not lost the word they were half-way through.
            pane.dismissed = true;
            pane.dismissed_at = pane.completion.slot;
            pane.asked = false;
        } else {
            // ABANDONING THE LINE ABANDONS THE DISMISSAL WITH IT. The dismissal was
            // made against a word; there is no longer a word, so keeping it would
            // leave the list hidden for the whole of the next command with nothing
            // on screen to explain why. (Measured: after Escape-Escape the next
            // three characters produced no list at all.)
            pane.input.clear(); // ...and the caret with it: `clear` moves both
            pane.dismissed = false;
        }
        break;
    case Act::kTerminalUp: move_completion(-1); return;   // the line did not change
    case Act::kTerminalDown: move_completion(+1); return; // ...so nothing is recomputed
    case Act::kTerminalComplete:
        // ONE KEY, ONE MEANING: "help me here". With a list on screen that is
        // taking the selected candidate; with nothing on screen it is asking
        // for one, which is the only gesture discovery needs because every
        // other entry point is ordinary typing.
        if (completion_selectable()) {
            accept_completion();
        } else {
            pane.asked = true;
            pane.dismissed = false;
        }
        break;
    default: break;
    }
    refresh_terminal();
}

// WL-TERM-01, WL-TERM-09 -- agents/workshop/terminal.md
// WL-GEO-01 -- agents/workshop/geometry.md
// WL-PTR-02 -- agents/workshop/pointer.md
// WL-PRESS-02 -- agents/workshop/press-chain.md
bool WorkshopWeave::terminal_press(const zengine::input::PointerButton& b) {
    TerminalPane& pane = session_.terminal;
    const Screen sc = screen_of(session_);

    // THE COMPLETION LIST, IF IT IS ON SCREEN. Its own condition is `paint_terminal`'s,
    // read from the same two flags, so a list a maker cannot see cannot be clicked.
    if (pane.completion.open && !pane.dismissed) {
        const CompletionPlace place =
            completion_place(sc, pane.completion.candidates.size() + 1 /*the heading*/);
        if (place.visible) {
            const surface::RegionFit fit =
                surface::fit_region(place.x, place.y, place.w, place.h, sc.text_advance_px,
                                    sc.text_line_px);
            const ProseAt at = prose_at(b.space, b.x, b.y, place.x, place.y, fit);
            if (at.understood && at.column >= 0 && at.column <= fit.columns &&
                at.row >= 0 && at.row < static_cast<std::int64_t>(place.rows)) {
                // ROW 0 IS THE HEADING and is not a candidate. A press on it is a press
                // on the list -- consumed, changing nothing -- rather than a press that
                // falls through to the input line underneath, which is not underneath
                // it at all.
                if (at.row >= 1) {
                    // THE SAME WINDOW THE ROWS WERE DRAWN WITH. `completion_first_shown`
                    // is the one answer to "which candidate is the first visible row",
                    // and it is read here rather than recomputed.
                    const std::size_t first =
                        completion_first_shown(pane.completion.selected, place.rows);
                    const std::size_t at_index =
                        first + static_cast<std::size_t>(at.row - 1);
                    if (at_index < pane.completion.candidates.size()) {
                        // ONE SELECTION, WHICHEVER HAND MOVED IT. There is no
                        // pointer-selected state beside the keyboard's: this writes the
                        // field Up/Down write, so the row a click chooses is a row Tab
                        // then accepts and the renderer cannot tell which happened.
                        pane.completion.selected = at_index;
                        return true;
                    }
                }
                return false; // on the list, on nothing choosable
            }
        }
    }

    // THE EDITABLE LINE. One region -- the pane's own -- and one row of it.
    const TerminalInputPlace place = terminal_input_place(sc);
    const ProseAt at = prose_at(b.space, b.x, b.y, place.region_x, place.region_y,
                                place.fit);
    if (at.understood && terminal_input_hit(place, at.column, at.row)) {
        const std::size_t was = pane.input.caret();
        const bool had_selection = pane.input.has_selection();
        // THROUGH THE WINDOW THE ROW WAS DRAWN WITH. A visible column names
        // `first_visible + offset` of the WHOLE authored line, never the offset alone --
        // that is the one subtraction a horizontal viewport adds to a hit test, and
        // leaving it out is right for exactly as long as no line is long enough to
        // scroll. The offset read here is the one the last repaint resolved, which is
        // the one the maker is looking at.
        const std::size_t target = terminal_caret_of_column(place, pane.input, at.column);
        //...AND A SECOND PRESS IN THE SAME WORD SELECTS IT, `info_press`'s twin
        // over the other instance of one component: the first press still places the
        // caret and still means exactly what it always did.
        const bool word = press_selects_word(b.modifiers, text_drag_place::kTerminalLine,
                                             pane.input, target);
        if (!word) {
            pane.input.place(target);
        }
        //...AND THE PRESS OPENS A SELECTION DRAG, `info_press`'s twin: the
        // caret just placed is the anchor, and motion until release extends from it. A
        // press that selected a WORD opens one too -- the anchor is the word's start, so
        // dragging from it extends the selection rather than replacing it, which is what
        // the component's own anchor/caret split already meant.
        session_.text_drag.active = true;
        session_.text_drag.place = text_drag_place::kTerminalLine;
        // The caret moving is what changes whether completion may be asked, so a press
        // that moved it has to reach `refresh_terminal` exactly as a caret key does. A
        // press that COLLAPSED a selection changed the picture too, even where the
        // caret stood still — the highlight has to leave the screen.
        if (word || pane.input.caret() != was || had_selection) {
            refresh_terminal();
            return true;
        }
        return false;
    }
    return false; // inside the mode, on none of its regions: consumed, and nothing moved
}

// WL-TERM-05 -- agents/workshop/terminal.md
bool WorkshopWeave::completion_selectable() const {
    const TerminalPane& pane = session_.terminal;
    return pane.open && pane.completion.open && !pane.dismissed &&
           !pane.completion.candidates.empty();
}

// WL-TERM-05 -- agents/workshop/terminal.md
void WorkshopWeave::move_completion(int by) {
    if (!completion_selectable()) {
        return;
    }
    Completion& comp = session_.terminal.completion;
    const std::size_t last = comp.candidates.size() - 1;
    if (by < 0) {
        comp.selected = comp.selected == 0 ? 0 : comp.selected - 1;
    } else {
        comp.selected = comp.selected >= last ? last : comp.selected + 1;
    }
}

// WL-TERM-05 -- agents/workshop/terminal.md
void WorkshopWeave::accept_completion() {
    if (!completion_selectable()) {
        return;
    }
    TerminalPane& pane = session_.terminal;
    const Completion& comp = pane.completion;
    const Candidate& c = comp.candidates[comp.selected];
    // `partial` is a TOKEN of this very line, so it can never be longer than the
    // line -- `tokenize` drops quote characters, which only ever makes a token
    // shorter than the text it came from, and a quoted token is refused by the
    // completer outright. The `min` is written anyway rather than as an `if`,
    // because the alternative to clamping is appending without stripping, which
    // is a doubled word on a line nobody could explain.
    const std::size_t typed =
        comp.partial.size() < pane.input.size() ? comp.partial.size() : pane.input.size();
    std::string line = pane.input.text();
    line.resize(line.size() - typed);
    line += c.insert;
    const std::size_t at = line.size();
    pane.input.set(std::move(line), at);
}

// WL-TERM-02, WL-TERM-04 -- agents/workshop/terminal.md
void WorkshopWeave::submit_terminal_line() {
    const std::string line = session_.terminal.input.text();
    session_.terminal.input.clear();
    // A SUBMITTED LINE ENDS BOTH PIECES OF COMPLETION STATE. Escape said "not for
    // this word" and Tab said "show me anyway"; the next line is neither, and a
    // maker who pressed one of them once should not find its effect still in
    // force three commands later.
    session_.terminal.dismissed = false;
    session_.terminal.asked = false;
    if (line.empty()) {
        return;
    }
    if (host_->terminal == nullptr) {
        // Nothing to author through, and nowhere to record the attempt: the
        // participant IS the transcript. So the tool's own notice line says
        // it, and it is visible the moment the pane closes.
        say("no terminal participant is mounted on this bus -- nothing was authored", true);
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
    const TerminalVerb* verb =
        tok.empty() ? nullptr : terminal_verb(tok[0].text);
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
    // wraps it across as many of its own rows as it needs (`detail::wrap`), so the length
    // of this string is a question about the GRAMMAR and never about the furniture. Before
    // wrapping it was fitted into one 56-cell row and a maker asking how to send a message got
    // `this pane speaks two verbs: \`send <addr> <Shape> <ver...` -- the answer truncated
    // at exactly the point it started being an answer.
    me.record_notice("this pane speaks two verbs, and `ask` takes the same form as `send`: "
                     "send <addr> <Shape> <version> [args] -- an address is #12 for one "
                     "weave, @office for whoever holds a role, or * for everyone");
}

// WL-TERM-03, WL-TERM-08 -- agents/workshop/terminal.md
// WL-TEXT-04 -- agents/workshop/text-box.md
void WorkshopWeave::refresh_terminal() {
    TerminalPane& pane = session_.terminal;
    const Screen sc = screen_of(session_);
    // THE ONE PLACE THE INPUT LINE'S HORIZONTAL WINDOW IS RECONCILED.
    //
    // It is here because this function runs on EVERY repaint -- which is the property
    // completion met as a trap and this needs as a guarantee. The window has to follow the
    // caret after a keystroke, after a press, after accepting a candidate AND after a
    // resize that changed nothing but the room; the first three are edits and the fourth
    // is not, so a hook on the edits would have missed exactly the witness §19 asks for.
    // Running it once per repaint answers all four with no special case, and it answers
    // them BEFORE `paint` and before the next press is mapped, so the picture and the
    // hit test are resolved against the same window.
    //
    // ABOVE THE `attached` RETURN ON PURPOSE: `paint_terminal` draws the pane whenever it
    // is OPEN, and `terminal_key` edits the line on the same condition, so a maker can
    // type into a pane with no participant mounted and must still be able to see where
    // they are. The capacity is `terminal_input_place`'s -- the same answer the painter
    // and the press consume, never a second count of the columns.
    pane.input.keep_caret_visible(terminal_input_place(sc).columns);
    pane.attached = host_->terminal != nullptr;
    pane.id = pane.attached ? host_->terminal->id() : loom::WeaveId{};
    pane.shown.clear();
    pane.earlier = 0;
    pane.dropped = 0;
    // TAKEN BEFORE THE CLEAR, because the clear is what this function does to
    // everything derived and the selection is the one thing that is not. See the
    // note below `complete_line` for what happens when these four are read after it.
    const LineSlot was_slot = pane.completion.slot;
    const std::string was_partial = pane.completion.partial;
    const bool was_open = pane.completion.open;
    const std::size_t was_selected = pane.completion.selected;
    pane.completion = Completion{};
    if (!pane.attached || !pane.open) {
        pane.dismissed = false; // a closed pane has no half-typed word to remember one for
        return;                 // nothing is painted from it, so nothing is copied
    }
    // WHAT COULD BE SAID NEXT, RECOMPUTED WITH THE LINE. It is derived from the
    // input and the participant's vocabulary and from nothing else, so it is
    // rebuilt rather than patched -- which is the same reason `shown` is a fresh
    // copy every time and not a list somebody maintains.
    //
    // AND IT AUTHORS NOTHING. `complete_line` takes the participant by const
    // reference; every method it reaches (`vocabulary()`, `describe()`,
    // `compose()`) is const, and the only path that authors goes through the
    // participant's own channel, which a const reference cannot touch. This is
    // the one call in this file that runs on every keystroke, and it is the one
    // that must never send.
    //
    // AND IT IS ASKED ABOUT THE END OF THE LINE, WHICH IS WHERE THE CARET HAS TO BE
    //. completer rests on an assumption that was free when the caret could
    // not move: the token being completed is the LAST one, so accepting is "drop what
    // has been typed of this token, append what it was going to be". With a caret in the
    // middle that edit would delete everything after it. The two honest repairs are to
    // teach the completer about a token under an arbitrary caret -- a second parser, on
    // a phase about carets and pointers -- or to say plainly that completion follows the
    // end of the line.
    //
    // AND IT SAYS IT OUT LOUD RATHER THAN GOING QUIET, which is own measured rule
    // arriving from a new direction: three different silences would otherwise render
    // identically, and a maker who moves the caret and watches the list vanish cannot
    // tell "not here" from "broken". So the list becomes a heading with no candidates in
    // it -- exactly the shape `send * s` already produces -- and a heading-only list is
    // transient and takes no gesture to dismiss.
    if (pane.input.at_end()) {
        pane.completion = complete_line(*host_->terminal, pane.input.text());
    } else {
        pane.completion.open = true;
        pane.completion.slot = read_command_line(pane.input.text()).slot;
        pane.completion.heading =
            "completion follows the END of the line -- this caret is inside it";
    }
    // THE SELECTION SURVIVES A REPAINT AND NOT A CHANGE OF QUESTION.
    //
    // This function runs on every repaint, not only when the line changes -- the pane
    // is a snapshot and a snapshot is only true when taken -- so a freshly computed
    // Completion arrives with `selected` at zero every time. Without this the arrow
    // keys appeared to do nothing at all: the move landed, the repaint immediately
    // after it undid the move, and the next Tab took the first candidate. (Found in
    // the live SDL run and reproduced headlessly, which is what said it was never the
    // driver -- and then reproduced a SECOND time, because the first repair read the
    // previous selection AFTER this function had already cleared it. A rebuild-from-
    // scratch function has exactly one place a survivor can be read, and it is the
    // top.)
    //
    // The question is the SLOT and the PARTIAL together. Same question, same
    // selection; a different word or a different part of the line is a new list and
    // starts at the top. Clamped either way, because a list can shrink under an
    // unchanged partial -- name a field and the field that was selected leaves.
    if (was_open && pane.completion.open && pane.completion.slot == was_slot &&
        pane.completion.partial == was_partial && !pane.completion.candidates.empty()) {
        const std::size_t last = pane.completion.candidates.size() - 1;
        pane.completion.selected = was_selected < last ? was_selected : last;
    }
    // A DISMISSAL BELONGS TO THE PART OF THE LINE IT WAS MADE IN. Moving on to
    // the next word is a new question, so the list comes back for it.
    if (pane.dismissed && pane.completion.slot != pane.dismissed_at) {
        pane.dismissed = false;
    }
    // AN UNTOUCHED LINE ASKS NOTHING. See `TerminalPane::asked`: the line is
    // empty immediately after a submit, and a list there covers the answer the
    // pane just gave. Typing is the gesture; Tab is the way to ask anyway.
    if (!pane.input.empty()) {
        pane.asked = false;
    } else if (!pane.asked) {
        pane.completion = Completion{};
    }
    // AN EMPTY LINE HAS ITS CARET AT THE END BY CONSTRUCTION, so the branch above and
    // the caret rule cannot disagree about it -- there is no position in an empty string
    // that is not both 0 and the end.
    // AS MANY ENTRIES AS THIS PANE CAN SHOW WHOLE, which is no longer the same as "as
    // many entries as it has rows": a line too long for the pane WRAPS rather
    // than being cut, so one entry can cost several rows. `entries_that_fit` is the one
    // place that arithmetic lives, and `paint_terminal` carries out the same choice with
    // the same call -- two answers here would be a pane whose omission marker lied.
    //
    // A row apiece is the floor, so `tail(rows)` is always at least as many entries as
    // can possibly fit, and the fitting only ever takes fewer. `sc` is the one resolved
    // at the top of this function -- the same screen the input line's window was
    // reconciled against, because two screens in one refresh is two answers.
    const loom::Transcript& record = host_->terminal->transcript();
    std::vector<loom::TranscriptEntry> newest = record.tail(sc.terminal_rows);
    const std::size_t fits = entries_that_fit(newest, sc.terminal_cols, sc.terminal_rows);
    newest.erase(newest.begin(),
                 newest.end() - static_cast<std::ptrdiff_t>(fits));
    pane.shown = std::move(newest);
    pane.earlier = record.size() - pane.shown.size();
    pane.dropped = record.evicted();
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
    case Act::kInfoUp: move_cursor(-1); break;
    case Act::kInfoDown: move_cursor(+1); break;
    case Act::kInfoEdit: begin_edit(); break;
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
    // BUILDING AND REALIZING stay two deliberate halves: realizing an
    // artifact is the one Builder gesture that changes what is running, and its
    // default is the chorded sibling of the plain build's.
    case Act::kBuild: build_now(mail, false); break;
    case Act::kBuildRealize: build_now(mail, true); break;
    case Act::kRecipeNext: choose_recipe(+1, mail); break;
    case Act::kRecipeBack: choose_recipe(-1, mail); break;
    case Act::kBuildFrontier: build_frontier(mail); break;
    // EDIT THE SOURCE the chosen recipe names -- the Builder-owned door into the
    // source editor, unbound without a Builder panel exactly as `b` is.
    case Act::kEditSource: edit_source(mail); break;
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
