// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s startup files, the host's conditions, and the surface and key handlers.
// Workshop law: agents/workshop/attention.md (+7 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

WorkshopWeave::WorkshopWeave(HostContext& host) : host_(&host) {}

// WL-KEY-07, WL-KEY-08 -- agents/workshop/keyboard.md
void WorkshopWeave::load_keymap(loom::Mail& mail) {
    if (keymap_loaded_) {
        return;
    }
    keymap_loaded_ = true;
    if (host_->keymap_path.empty()) {
        keymap_standing_ = "no keymap file -- the shipped bindings stand";
        return;
    }
    if (!std::filesystem::exists(host_->keymap_path)) {
        keymap_standing_ = "not written yet -- the shipped bindings stand; a row written there "
                           "applies at the next launch";
        return;
    }
    // READ ONCE, AND THE BYTES KEPT: they are the baseline an edit's write compares the file
    // against, so a change by another hand since this read is seen rather than overwritten.
    const persist::FileText read = persist::read_file(
        host_->keymap_path, keymap_persist::kMaxKeymapBytes, "a Workshop keymap");
    const keymap_persist::LoadedKeymap loaded =
        read.outcome.accepted ? keymap_persist::from_text(read.text)
                              : keymap_persist::LoadedKeymap{read.outcome, {}};
    if (read.outcome.accepted) {
        keymap_bytes_ = read.text;
        keymap_file_present_ = true;
    }
    if (!loaded.outcome.accepted) {
        // A standing wall, said as one: every later launch meets it, so it is a condition under
        // its own key, retracted by nobody, since nothing in a run makes the file readable.
        keymap_bad_ = true;
        keymap_standing_ = "refused, so the shipped bindings stand: " + loaded.outcome.refusal;
        session_.conditions.establish(
            Condition{kKeymapWallKey, "keymap refused -- default bindings stand",
                      loaded.outcome.refusal, surface::role::kAlert, std::string()});
        return;
    }
    session_.keymap = loaded.keymap;
    // And every pane that declared rows before the file loaded is joined again under it, so both
    // load orders end the same way: the file in force, and a colliding pane's rows refused in
    // words (WL-KEY-15).
    // A ROW FOR AN ID WHOSE OWNER CHANGED IS READ AS ITS SUCCESSOR, and the load says so, naming
    // the rename that would make the file say what it means (`kRenamedActions`).
    std::string renamed;
    for (const AuthoredOverride& o : session_.keymap.authored) {
        if (const char* now = renamed_to(o.action)) {
            renamed += (renamed.empty() ? "" : "; ") + ("`" + o.action + "` is read as `" +
                                                        std::string(now) + "` -- rename it there");
        }
    }
    // ...AND A ROW FOR AN ID THAT RETIRED IS SAID AS ONE (`kRetiredActions`): kept byte for byte,
    // answered by nothing, and named with what took it, so a maker is not left wondering whether
    // a pane might yet declare it.
    std::string retired;
    for (const AuthoredOverride& o : session_.keymap.authored) {
        if (const char* with = retired_with(o.action)) {
            const char* instead = retired_instead(o.action);
            retired += (retired.empty() ? "" : "; ") +
                       ("`" + o.action + "` retired with " + std::string(with) +
                        " -- kept, and nothing answers it" +
                        (instead != nullptr && *instead != 0
                             ? " (" + std::string(instead) + ")"
                             : std::string()));
        }
    }
    keymap_standing_ = "applied -- " + std::to_string(session_.keymap.authored.size()) +
                       " authored row" + (session_.keymap.authored.size() == 1 ? "" : "s") +
                       (renamed.empty() ? std::string() : "; " + renamed) +
                       (retired.empty() ? std::string() : "; " + retired);
    if (!renamed.empty()) {
        session_.keymap.note += (session_.keymap.note.empty() ? "" : "; ") + renamed;
    }
    if (!retired.empty()) {
        session_.keymap.note += (session_.keymap.note.empty() ? "" : "; ") + retired;
    }
    std::string refused;
    // THE APPLICATION'S ROWS FIRST, because a pane is judged against them (WL-DESK-07): the file
    // replaced the whole map, and the declaration the desktop has in force is owed the maker's
    // overrides now rather than whenever the desktop next happens to speak.
    rejoin_app_rows(refused, mail);
    rejoin_pane_rows(refused, mail);
    if (!session_.keymap.authored.empty() || session_.keymap.legend != legend_mode::kDefault) {
        keymap_word_ = "keymap " + host_->keymap_path + " applied -- " +
                       std::to_string(session_.keymap.authored.size()) + " override" +
                       (session_.keymap.authored.size() == 1 ? "" : "s");
        if (!session_.keymap.note.empty()) {
            keymap_word_ += "; " + session_.keymap.note;
        }
    }
    if (!refused.empty()) {
        if (!keymap_word_.empty()) {
            keymap_word_ += "; ";
        }
        keymap_word_ += refused;
    }
}

// WL-FOCUS-11 -- agents/workshop/focus.md
void WorkshopWeave::load_prefs() {
    if (prefs_loaded_) {
        return;
    }
    prefs_loaded_ = true;
    if (host_->prefs_path.empty()) {
        return;
    }
    if (!std::filesystem::exists(host_->prefs_path)) {
        return;
    }
    const prefs_persist::LoadedPrefs loaded = prefs_persist::load_file(host_->prefs_path);
    if (!loaded.outcome.accepted) {
        // The keymap wall's twin: `prefs_bad_` blocks every later write. The condition is the
        // standing half; a toggle's refusal stays an event about the press.
        prefs_bad_ = true;
        session_.conditions.establish(
            Condition{kPrefsWallKey, "preferences refused -- defaults stand",
                      loaded.outcome.refusal, surface::role::kAlert, std::string()});
        return;
    }
    session_.pane_titles = loaded.titles_shown;
}

// WL-ATTN-02 -- agents/workshop/attention.md
void WorkshopWeave::speak_startup_notes(loom::Mail& mail) {
    if (startup_spoken_) {
        return;
    }
    startup_spoken_ = true;
    std::string word;
    // AN OBJECT DOCUMENT THIS RUN WAS POINTED AT, SAID ONCE AND LEFT ALONE (WL-DOC-22): the file is
    // a maker's, the canvas that read it retired, and nothing here opens, rewrites or deletes it.
    const std::string retired =
        host_->retired_document.empty()
            ? std::string()
            : "object document " + host_->retired_document +
                  " left as it is -- the object canvas retired, and nothing here reads or "
                  "writes it";
    const std::string* parts[] = {&keymap_word_, &host_->transition_note, &retired};
    for (const std::string* part : parts) {
        if (part->empty()) {
            continue;
        }
        if (!word.empty()) {
            word += "; ";
        }
        word += *part;
    }
    if (word.empty()) {
        return;
    }
    say(word, false);
    repaint(mail);
}

// WL-ATTN-01, WL-ATTN-02 -- agents/workshop/attention.md
void WorkshopWeave::take_host_conditions() {
    conditions_taken_ = host_->conditions_generation;
    for (const Condition& c : host_->standing_conditions) {
        session_.conditions.establish(c);
    }
}

void WorkshopWeave::on(const zengine::surface::SurfaceReady&, loom::Mail& mail) {
    (void)mail.as_role(kWorkshopProvider).publish(PaneCatalogRequested{});
    // The first picture of a run is Workshop's floor: the smallest room it is honest in comes
    // first and the room it wants back second, so a maker can always shrink the window.
    load_keymap(mail);
    // The prefs beside it, BEFORE the first paint: the first band and the
    // first pane headers a maker reads are already wearing their own preference.
    load_prefs();
    // ...and the maker's own pane before the session is taken back: `apply_setup` seats a
    // reference only if it resolves then, so the definition opens first and the session finds
    // it as it finds a built-in.
    load_pane_definition(mail);
    // ...and whatever the host already knew was standing, so the first picture
    // of the run already carries every condition this launch is going to have.
    take_host_conditions();
    repaint(mail);
    restore_last_session(mail);
    speak_startup_notes(mail);
}

// WL-MAKER-08, WL-MAKER-09 -- agents/workshop/maker-pane.md
void WorkshopWeave::load_pane_definition(loom::Mail& mail) {
    if (pane_loaded_) {
        return;
    }
    pane_loaded_ = true;
    const std::string path = host_pane_path();
    if (path.empty()) {
        return;
    }
    if (!std::filesystem::exists(path)) {
        return;
    }
    open_maker_pane(path, mail);
}

// WL-MAKER-08 -- agents/workshop/maker-pane.md
std::string WorkshopWeave::host_pane_path() const {
    if (host_->pane_path.empty()) {
        return std::string();
    }
    return persist::resolved_against(host_->project_dir, host_->pane_path);
}

// WL-GEO-08 -- agents/workshop/geometry.md
void WorkshopWeave::on(const zengine::surface::SurfaceExtent& e, loom::Mail& mail) {
    if (!adopt_screen(session_, e.width, e.height, e.text_advance_px, e.text_line_px,
                      e.cell_px)) {
        return;
    }
    // The normal window's room follows the screen, except while this run's medium says the window
    // is maximized (placement arrives before extent on its beat). A maximized flag restored from
    // a file does not gate: this run's resizes are this run's to remember.
    if (!(medium_placed_ && session_.place_maximized)) {
        session_.normal_w = session_.screen_w;
        session_.normal_h = session_.screen_h;
    }
    // The composition is reconciled against the room it now has: growth may open a pane waiting
    // for room, and a shrink closes a presentation through the ordinary door and leaves the
    // authored reference. `apply_setup` is the one path either way.
    apply_setup(mail);
    repaint(mail);
}

// WL-SESSION-08, WL-SESSION-09 -- agents/workshop/session-restore.md
void WorkshopWeave::on(const zengine::surface::SurfacePlacement& p, loom::Mail&) {
    medium_placed_ = true;
    session_.placement_known = true;
    session_.place_x = p.x;
    session_.place_y = p.y;
    session_.place_maximized = p.maximized;
}

// WL-SESSION-13 -- agents/workshop/session.md
void WorkshopWeave::on(const zengine::surface::SurfaceCloseRequested&, loom::Mail& mail) {
    quit(mail);
}

// WL-KEY-03, WL-KEY-05, WL-KEY-12 -- agents/workshop/keyboard.md
void WorkshopWeave::on(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    if (duplicate_input(mail)) return;
    // WHILE THE ROOM IS BEING ASKED WHETHER THIS WORKSHOP MAY END, NOTHING IS ROUTED. Held,
    // and replayed if the answer is no (`quit`).
    if (quitting_) {
        HeldInput held;
        held.kind = HeldInput::Kind::kKey;
        held.key = k;
        (void)hold_input(std::move(held));
        return;
    }
    ++gestures_;
    gesture_actor_ = input_actor_;
    if (!carried_.data.empty() && k.scancode == input::scan::kEscape &&
        input_actor_.known && input_actor_.local == carried_.actor.local &&
        input_actor_.participant == carried_.actor.participant) {
        carried_ = {};
        say("Reference put down", false);
        repaint(mail);
        return;
    }
    // THE CONTEXT IS RESOLVED ONCE, AT ENTRY, and every decision this turn -- the
    // above-mode arm, the swallow, the chain -- spends the same answer, so a mode a
    // dispatch arm opens cannot change what THIS keystroke meant.
    const KeyContext ctx = keyboard_context(session_);
    // The swallow belongs to one moment: cleared on every key, armed only when this keystroke is
    // consumed as a binding whose key also enters text (`expected_text_of`); an unmatched
    // expectation eats nothing.
    swallow_text_.clear();
    if (ctx == KeyContext::kContext && session_.presented.open) {
        // Every key a presented menu takes is its presenter's, so the character it produced is
        // part of that act, not a second one.
        swallow_text_ = expected_text_of(k.scancode, k.modifiers);
    } else if (session_.keymap.action_for(ctx, k.scancode, k.modifiers, keyboard_pane()) !=
        Act::kNone) {
        swallow_text_ = expected_text_of(k.scancode, k.modifiers);
    } else if (session_.keymap.app_action_for(app_precedence::kAboveModes, ctx, k.scancode,
                                             k.modifiers, keyboard_pane()) != nullptr) {
        // An application row is a binding too (WL-DESK-07): a desktop's launch on a bare letter
        // must not also type that letter.
        swallow_text_ = expected_text_of(k.scancode, k.modifiers);
    } else if (ctx == KeyContext::kPane &&
               session_.keymap.pane_action_for(keyboard_pane(), k.scancode, k.modifiers) !=
                   nullptr) {
        // A PANE'S OWN ROW IS A BINDING TOO (WL-KEY-15): the keystroke crosses as the
        // resolved id, and the character it produced belongs to the trigger, not to the
        // pane's field.
        swallow_text_ = expected_text_of(k.scancode, k.modifiers);
    }
    // Only the rows declared above the modes are answered here; `workshop.quit`'s ordinary `q`
    // row still travels the chain, behind every mode.
    switch (session_.keymap.above_mode_action(ctx, k.scancode, k.modifiers, keyboard_pane())) {
    case Act::kQuit:
        // A quit REFUSED says so on the notice line, which has to be painted to be read;
        // a quit that proceeded publishes one last unchanged frame on its way out, which
        // costs nothing anybody sees; a quit that is ASKING paints the desk as it is.
        quit(mail);
        repaint(mail);
        return;
    default: break;
    }
    // The chain is `keyboard_context`'s answer; this switch names the owner it resolved. A copy or
    // paste anywhere below is noticed once, here, by comparing the clipboard's counters around the
    // chain: a copy publishes `ClipboardCopy`, a paste asks the Skin for the current value
    // (`begin_clipboard_paste`). The application's own rows come first, above every mode and
    // below this host's own above-mode rows; a pane keeps a gesture only by declaring it owns
    // it (the laws WL-DESK-07 and WL-ARR-15), never by silence.
    if (const AppRow* row = session_.keymap.app_action_for(app_precedence::kAboveModes, ctx,
                                                           k.scancode, k.modifiers,
                                                           keyboard_pane())) {
        request_app_action(row->id, mail);
        repaint(mail);
        return;
    }
    const std::uint64_t copied_before = session_.clipboard.writes;
    const std::uint64_t pastes_before = session_.clipboard.paste_requests;
    bool crossed = true; // whether a key sent to a pane crossed at all (`external_key`)
    switch (ctx) {
    case KeyContext::kArrangePane:
    case KeyContext::kArrangeDesk:
    case KeyContext::kArrangeReset: arrange_key(k, mail); break;
    case KeyContext::kNaming: naming_key(k, mail); break;
    case KeyContext::kContext: context_key(k, mail); break;
    case KeyContext::kPane: crossed = external_key(keyboard_pane(), k, mail); break;
    default: command(k, mail); break;
    }
    // Escape's final meaning is asked last, after the resolved context had the key. A bare Escape
    // no binding claimed, where the keys are held by a list, by nothing, or by a pane that took no
    // key, requests the desktop's default-class row (the law WL-DESK-02); a pane that took the key
    // answers with `PaneEscapeUnspent`, and a place a maker types into keeps Escape. Not a keymap
    // action: a recovery gesture must not be authorable into a lockout.
    if ((default_row_context(ctx) || (ctx == KeyContext::kPane && !crossed)) &&
        session_.keymap.action_for(ctx, k.scancode, k.modifiers, keyboard_pane()) ==
            Act::kNone) {
        if (const AppRow* row = session_.keymap.app_action_for(
                app_precedence::kDefault, ctx, k.scancode, k.modifiers, keyboard_pane())) {
            request_app_action(row->id, mail);
        }
    }
    if (session_.clipboard.writes != copied_before) {
        mail.publish(zengine::surface::ClipboardCopy{session_.clipboard.text});
    }
    if (session_.clipboard.paste_requests != pastes_before) {
        begin_clipboard_paste(mail);
    }
    repaint(mail);
}

} // namespace zengine::workshop
