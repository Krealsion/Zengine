// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s sections -- the startup files, the host's conditions, and the
// surface handlers -- compiled once into `zengine-workshop-logic` and linked by the host and
// every suite; the declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/attention.md (+7 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

WorkshopWeave::WorkshopWeave(HostContext& host) : host_(&host) {
    // The document a maker opens onto. Deliberately boring, and deliberately
    // TWO rectangles sharing nothing but a name pattern -- `panel` and
    // `panel` would be the same object in the old builder, and here they are
    // #1 and #2. The wide one is authored as a SHARE so the very first screen
    // already shows an authored intent and its resolved value side by side.
    doc::add(state_, "panel", 3, 2, ui::Extent{ui::kExtentPercent, 60},
             ui::Extent{ui::kExtentCells, 6});
    doc::add(state_, "panel", 6, 10, ui::Extent{ui::kExtentCells, 14},
             ui::Extent{ui::kExtentCells, 4});
    open_on_first();
}

// WL-KEY-07, WL-KEY-08 -- agents/workshop/keyboard.md
void WorkshopWeave::load_keymap() {
    if (keymap_loaded_) {
        return;
    }
    keymap_loaded_ = true;
    if (host_->keymap_path.empty()) {
        return;
    }
    if (!std::filesystem::exists(host_->keymap_path)) {
        return;
    }
    const keymap_persist::LoadedKeymap loaded =
        keymap_persist::load_file(host_->keymap_path);
    if (!loaded.outcome.accepted) {
        // A STANDING WALL, AND IT IS SAID AS ONE. The file is still refused
        // an hour later and every later launch meets the same wall, so this is a
        // CONDITION and not a sentence about a moment -- established under its own key,
        // with the loader's own refusal as the explanation, and retracted by nobody
        // because nothing in a run can make an unreadable file readable.
        keymap_bad_ = true;
        session_.conditions.establish(
            Condition{kKeymapWallKey, "keymap refused -- default bindings stand",
                      loaded.outcome.refusal, surface::role::kAlert, std::string()});
        return;
    }
    session_.keymap = loaded.keymap;
    if (!session_.keymap.authored.empty() || session_.keymap.legend != legend_mode::kDefault) {
        keymap_word_ = "keymap " + host_->keymap_path + " applied -- " +
                       std::to_string(session_.keymap.authored.size()) + " override" +
                       (session_.keymap.authored.size() == 1 ? "" : "s");
        if (!session_.keymap.note.empty()) {
            keymap_word_ += "; " + session_.keymap.note;
        }
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
        // THE KEYMAP WALL'S TWIN, one file over, and this one is load-bearing beyond
        // the sentence: `prefs_bad_` blocks every later write, so a maker who toggles a
        // preference is told again, in different words, at each toggle. The CONDITION
        // is the standing half -- true from this load until the process ends -- and the
        // toggle's refusal stays an event, because it is about the press.
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
    for (const std::string* part : {&keymap_word_, &host_->transition_note}) {
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
    for (const Condition& c : host_->standing_conditions) {
        session_.conditions.establish(c);
    }
}

void WorkshopWeave::on(const zengine::surface::SurfaceReady&, loom::Mail& mail) {
    (void)mail.as_role(kWorkshopProvider).publish(PaneCatalogRequested{});
    // THE FIRST PICTURE OF A RUN IS WORKSHOP'S FLOOR, AND THAT IS LOAD-BEARING.
    //
    // A medium that has not been told anything has only this picture to size itself
    // from, and whatever it makes of it, it must not be that Workshop can never again be
    // smaller than the window its maker happened to leave open last night. So the run's
    // FIRST statement about how much room it wants is the smallest room it is honest in,
    // and the room it is trying to get BACK is the second -- a want, not a floor.
    // Reversing the two costs a maker the ability to shrink their window, which is a
    // stranger thing to lose to a continuity feature than anything it could have bought.
    load_keymap();
    // The prefs beside it, BEFORE the first paint: the first band and the
    // first pane headers a maker reads are already wearing their own preference.
    load_prefs();
    //...and the maker's own places, so a marks file this run cannot read is a
    // condition the FIRST picture already carries rather than one discovered whenever
    // the browser happens to open. It has its own once-guard, so a run that reaches the
    // browser before any surface exists reads them there instead, exactly once.
    load_marks();
    //...AND THE MAKER'S OWN PANE, BEFORE THE SESSION IS TAKEN BACK -- the
    // ordering is the whole of the relaunch story. The session's desks name a
    // maker-made pane by its durable reference, and `apply_setup` seats a reference
    // only if it resolves at that moment; a definition opened after the restore would
    // leave the pane a maker saved yesterday counted unresolved on the very row it
    // came back on. So identity is established first, from its own file, through the
    // one open door; the session then finds it exactly as it finds a built-in.
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

// WL-DOC-17 -- agents/workshop/document.md; WL-GEO-08 -- agents/workshop/geometry.md
void WorkshopWeave::on(const zengine::surface::SurfaceExtent& e, loom::Mail& mail) {
    if (!adopt_screen(session_, e.width, e.height, e.text_advance_px, e.text_line_px,
                      e.cell_px)) {
        return;
    }
    // THE NORMAL WINDOW'S ROOM FOLLOWS THE SCREEN, EXCEPT WHILE THIS RUN'S MEDIUM SAYS
    // THE WINDOW IS MAXIMIZED. The medium reports placement BEFORE extent on
    // its beat (skin.hpp says why once), so by the time a maximized room arrives the
    // gate is already closed and the remembered normal viewport survives to the save.
    // A run whose medium never reports placement -- every terminal -- never gates, and
    // a maximized flag merely RESTORED from a file must not gate either: that is last
    // run's window, and this run's resizes are this run's to remember.
    if (!(medium_placed_ && session_.place_maximized)) {
        session_.normal_w = session_.screen_w;
        session_.normal_h = session_.screen_h;
    }
    // THE ROWS ARE REBUILT AND A LIVE DRAFT IS CARRIED ACROSS. The resolved row
    // closes over the extent it resolves against, so the rebuild is not optional -- but
    // this is the ONE rebuild that happens for a reason having nothing to do with the
    // maker. A window dragged is not a gesture aimed at the inspector, and earlier
    // measured it on the pristine tree it silently threw away whatever was half-typed
    // into a property, its refusal and the cursor with it. Every OTHER caller of
    // `rebuild_rows` follows a change of selection or of document, where dropping the
    // draft is the right answer and carrying it would put it on a different object.
    refocus_keeping_draft(state_, session_);
    // AND THE COMPOSITION IS RECONCILED AGAINST THE ROOM IT NOW HAS. A screen
    // that grew may have gained an overlay slot, and one that shrank may have lost the
    // one a panel was standing in -- so this is the second reason a reconcile happens
    // and the only one that is not a maker's gesture. Growth opens an authored pane that
    // was waiting for room; a shrink closes the presentation through the ordinary close
    // door, destroys its cache, and leaves the authored reference exactly where it was.
    // `apply_setup` is the one path either way, so a resize cannot open or close a panel
    // by some route the picker and a restore do not share.
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
void WorkshopWeave::on(const zengine::surface::SurfaceCloseRequested&, loom::Mail&) { quit(); }

// WL-KEY-03, WL-KEY-05, WL-KEY-12 -- agents/workshop/keyboard.md
void WorkshopWeave::on(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    // THE CONTEXT IS RESOLVED ONCE, AT ENTRY, and every decision this turn -- the
    // above-mode arm, the swallow, the chain -- spends the same answer, so a mode a
    // dispatch arm opens cannot change what THIS keystroke meant.
    const KeyContext ctx = keyboard_context(session_);
    // THE SWALLOW BELONGS TO ONE MOMENT: cleared on every key, armed only when this
    // keystroke is consumed as an application binding whose key also enters text
    // (`expected_text_of` derives the owed character from the binding -- the
    // generalization of the three hard-coded `" "`/`"s"`/`"w"` sites the keymap research found,
    // and deliberately not a swallow-the-next-text rule: an unmatched or absent
    // expectation eats nothing).
    swallow_text_.clear();
    if (session_.keymap.action_for(ctx, k.scancode, k.modifiers) != Act::kNone) {
        swallow_text_ = expected_text_of(k.scancode, k.modifiers);
    }
    // ONLY THE ROWS DECLARED ABOVE THE MODES ARE ANSWERED HERE. `workshop.quit`'s
    // ordinary `q` row resolves to the same action and still travels the chain --
    // which is what keeps the hotkey view's swallow, and every mode's ownership,
    // ahead of it.
    switch (session_.keymap.above_mode_action(ctx, k.scancode, k.modifiers)) {
    case Act::kQuit:
        // A quit REFUSED for unsaved source says so on the notice line, which has to
        // be painted to be read; a quit that proceeded publishes one last unchanged
        // frame on its way out, which costs nothing anybody sees.
        quit();
        repaint(mail);
        return;
    case Act::kTerminalToggle:
        toggle_terminal();
        repaint(mail);
        return;
    case Act::kSaveDocument:
        save_document();
        repaint(mail);
        return;
    case Act::kOpenDocument:
        load_document();
        repaint(mail);
        return;
    case Act::kHotkeys:
        toggle_hotkeys();
        repaint(mail);
        return;
    case Act::kAttention:
        toggle_attention();
        repaint(mail);
        return;
    default: break;
    }
    // THE HOTKEY VIEW IS KEYS-MODAL WHILE IT IS OPEN: the five arms above still
    // answer (its own toggle is one of them), and everything else is the view's to
    // spend or swallow -- a maker reading a key list must not be executing it. The
    // context beneath is untouched, which is why `ctx` above still names it.
    if (session_.hotkeys.open) {
        hotkeys_key(k);
        repaint(mail);
        return;
    }
    // THE CHAIN IS `keyboard_context`'S ANSWER NOW. Its order -- and every
    // recorded rationale behind it: the modes that own the keyboard whole, the
    // reachability arguments that are written down anyway, the pressed-into-LAST
    // symmetry that puts a focused pane above a live draft, `keyboard_pane` resolved
    // fresh with nothing to clear -- lives with the resolver, where the paint path
    // and the paste mirror read the same answer. This switch is what remains of four
    // hand-copies of that order: which owner the resolved context names.
    //
    // A COPY ANYWHERE BELOW IS SAID TO THE PROCESS ONCE, HERE. The component
    // bumps the clipboard's `writes` exactly when a copy or cut took text, so one
    // comparison around the whole chain notices it whichever consumer it happened in —
    // three handlers each publishing would be the fourth-copy accident arriving in the
    // routing. What is published is `ClipboardCopy`: the Skin offers it to the
    // platform's clipboard and every other text-holding participant mirrors it.
    //
    // A PASTE ANYWHERE BELOW IS ASKED FOR ONCE, HERE, THE SAME WAY. The
    // component bumps `paste_requests` instead of pasting, because the value a paste
    // means is the clipboard's CURRENT value and only this owner can obtain it — a
    // read performed BECAUSE this paste was requested, never a mirror kept fresh by
    // watching. The same one comparison notices it, `paste_owner_now()` (a derivation
    // of the resolved context not a second spelling of the chain) says
    // which draft asked, and `begin_clipboard_paste` opens the conversation with the
    // Skin. The text lands a turn later, in that draft or nowhere
    // (`on(ClipboardText)`).
    const std::uint64_t copied_before = session_.clipboard.writes;
    const std::uint64_t pastes_before = session_.clipboard.paste_requests;
    switch (ctx) {
    case KeyContext::kTerminal: terminal_key(k); break;
    case KeyContext::kArrangePane:
    case KeyContext::kArrangeDesk:
    case KeyContext::kArrangeReset: arrange_key(k, mail); break;
    case KeyContext::kNaming: naming_key(k, mail); break;
    case KeyContext::kPaneNaming: pane_naming_key(k, mail); break;
    case KeyContext::kPicker: picker_key(k, mail); break;
    case KeyContext::kAttention: attention_key(k); break;
    case KeyContext::kContext: context_key(k, mail); break;
    case KeyContext::kPane: external_key(keyboard_pane(), k, mail); break;
    case KeyContext::kEditor: editor_key(k); break;
    case KeyContext::kFiles: files_key(k, mail); break;
    case KeyContext::kPaneEditor: pane_editor_key(k, mail); break;
    case KeyContext::kDraft: editing_key(k, mail); break;
    default: command(k, mail); break;
    }
    // ESCAPE'S FINAL MEANING IS TO PUT THE SELECTED PANE DOWN. It is asked
    // LAST, after the resolved context has had the key: every mode, overlay and draft
    // answers Escape with a row of its own (`picker.close`, `draft.cancel`,
    // `manage.close`, `context.back`, `attention.close`, `naming.cancel`,
    // `terminal.back`), the hotkey view took it further up -- and a bare Escape that
    // arrives here in a context where the keys are held by a LIST or by NOTHING, with
    // no binding claiming it, is an Escape nothing more specific owned. Then, if a pane
    // is selected, the selection is the layer it sheds: the press-elsewhere gesture's
    // own two lines, spent without needing an unoccupied pixel to press. Nothing else
    // moves -- no pane closes, no rank, no geometry, no Pane Editor subject, no
    // provider state, no file.
    //
    // A PLACE A MAKER TYPES INTO KEEPS ESCAPE WHILE IT HOLDS THE KEYS
    // (`escape_may_shed_selection`). The source editor's Escape is a pinned no-op
    // -- a maker's habitual Esc must not hand the next `d` to command mode. A
    // focused external pane has already been sent the key and Workshop cannot see
    // whether it spent it: the seam carries no `consumed`, by design, and the
    // shipped Composer does spend it (its form goes back to its catalog and the maker
    // keeps typing). So the pane keeps its keys, and the way out of either is
    // the way in: press a pane that takes no text -- every desk has one, Layouts --
    // or the workspace, then Escape.
    //
    // IT IS DELIBERATELY NOT A KEYMAP ACTION, the hotkey view's own reason: a recovery
    // gesture must not be authorable into a lockout. A maker who binds Escape to an
    // action in one of these contexts has said what Escape means there; their binding
    // answered above and this line does not.
    if (k.scancode == input::scan::kEscape && k.modifiers == input::mod::kNone &&
        escape_may_shed_selection(ctx) &&
        session_.keymap.action_for(ctx, k.scancode, k.modifiers) == Act::kNone &&
        session_.panels.selected != kNoPaneKind) {
        unselect_pane();
    }
    if (session_.clipboard.writes != copied_before) {
        mail.publish(zengine::surface::ClipboardCopy{session_.clipboard.text});
    }
    if (session_.clipboard.paste_requests != pastes_before) {
        begin_clipboard_paste(mail);
    }
    repaint(mail);
}

void WorkshopWeave::toggle_hotkeys() { session_.hotkeys.open = !session_.hotkeys.open; }

// WL-KEY-11 -- agents/workshop/keyboard.md
void WorkshopWeave::hotkeys_key(const zengine::input::KeyPressed& k) {
    if (k.scancode == input::scan::kEscape && k.modifiers == input::mod::kNone) {
        session_.hotkeys.open = false;
    }
}

// WL-ATTN-09, WL-ATTN-10 -- agents/workshop/attention.md
void WorkshopWeave::toggle_attention() {
    session_.attention.open = !session_.attention.open;
    session_.attention.cursor = 0;
}

// WL-ATTN-08, WL-ATTN-09 -- agents/workshop/attention.md
void WorkshopWeave::attention_key(const zengine::input::KeyPressed& k) {
    AttentionView& view = session_.attention;
    std::vector<Condition> shown = attention_shown(session_, frontier_now());
    if (view.cursor >= shown.size()) {
        view.cursor = shown.empty() ? 0 : shown.size() - 1;
    }
    switch (session_.keymap.action_for(KeyContext::kAttention, k.scancode, k.modifiers)) {
    case Act::kAttentionUp:
        if (view.cursor > 0) {
            --view.cursor;
        }
        break;
    case Act::kAttentionDown:
        if (view.cursor + 1 < shown.size()) {
            ++view.cursor;
        }
        break;
    case Act::kAttentionDismiss:
        if (view.cursor < shown.size()) {
            const std::string hidden = shown[view.cursor].compact;
            view.dismiss(shown[view.cursor]);
            // AN EVENT, SAID AS ONE. What just happened is that a maker hid a
            // presentation; what remains true is the condition, which is why the
            // sentence is about the gesture and not about the subject.
            say("hidden -- " + hidden + " is still true", false);
        }
        break;
    case Act::kAttentionClose: view.open = false; break;
    default:
        // THE KEY THAT OPENED IT CLOSES IT -- the picker's and the terminal overlay's
        // shared rule, following the OPENER'S effective binding wherever a maker moved
        // it. The toggle is a global, so it is answered above this switch and never
        // reaches here; this arm is what makes a remapped opener still close.
        if (session_.keymap.matches(Act::kAttention, k.scancode, k.modifiers)) {
            view.open = false;
        }
        break;
    }
}

} // namespace zengine::workshop
