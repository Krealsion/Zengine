// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s sections -- the contextual-action surface and the pointer --
// compiled once into `zengine-workshop-logic` and linked by the host and every suite; the
// declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/contextual.md (+9 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- What can I do with this? The contextual-action surface ---------------

// WL-CTX-01 -- agents/workshop/contextual.md
// WL-CTX-01 -- agents/workshop/contextual.md
void WorkshopWeave::open_context_at(const PointedAt& at) {
    ContextMenu next;
    next.open = true;
    // THE PRESS'S OWN CELL IS THE ANCHOR: the surface opens beside the hand
    // that asked, on both media at the cell grain -- the composition is settled in
    // cells before any metric is consulted, own medium-independence rule.
    // The bounds stay derived; only the gesture's place is captured.
    next.anchored = true;
    next.anchor_x = at.cell.x;
    next.anchor_y = at.cell.y;
    const Occupancy here =
        occupied_at(session_.panels, session_.setup.active, screen_of(session_), at);
    if (here.occupied) {
        // The setup row that RESOLVES to the pointed presentation -- the durable
        // identity, never the kind handle (`arrange_press`'s own walk). The picker's
        // rectangle resolves to no row and falls through to the room.
        for (const SetupPane& row : session_.setup.active.panes) {
            const std::optional<std::int64_t> named =
                resolve_pane(row.ref, session_.panels);
            if (named.has_value() && *named == here.kind) {
                next.subject = context_subject::kPane;
                next.pane = row.ref;
                break;
            }
        }
    } else {
        const std::int64_t id = object_at(state_, session_, workspace_cell_x(at.cell.x),
                                          workspace_cell_y(at.cell.y));
        if (id != 0) {
            next.subject = context_subject::kObject;
            next.object = id;
        }
    }
    session_.context = next;
}

// WL-TAB-12 -- agents/workshop/tab-run.md
void WorkshopWeave::open_context_on_layout(const PointedAt& at, std::size_t layout) {
    ContextMenu next;
    next.open = true;
    next.anchored = true;
    next.anchor_x = at.cell.x;
    next.anchor_y = at.cell.y;
    next.subject = context_subject::kLayout;
    next.layout = layout;
    session_.context = next;
}

// WL-CTX-01 -- agents/workshop/contextual.md
void WorkshopWeave::open_context_ambient() {
    ContextMenu next;
    next.open = true;
    if (doc::find(state_, session_.selected) != nullptr) {
        next.subject = context_subject::kObject;
        next.object = session_.selected;
    }
    session_.context = next;
}

void WorkshopWeave::close_context() { session_.context = ContextMenu{}; }

void WorkshopWeave::context_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    ContextMenu& menu = session_.context;
    const std::vector<ContextEntry> rows = context_population(menu.subject, menu.group);
    menu.cursor = context_cursor_bound(menu.cursor, rows.size());
    switch (session_.keymap.action_for(KeyContext::kContext, k.scancode, k.modifiers)) {
    case Act::kContextUp:
        if (menu.cursor > 0) {
            --menu.cursor;
        }
        break;
    case Act::kContextDown:
        if (menu.cursor + 1 < rows.size()) {
            ++menu.cursor;
        }
        break;
    case Act::kContextChoose: choose_context_row(mail); break;
    case Act::kContextBack:
        // ESCAPE DOES THE APPROPRIATE SMALLER THING: out of an open group, else out
        // of the surface -- pane management's done/close pair, in a surface whose
        // depth is presentation state rather than a submode.
        if (!menu.group.empty()) {
            leave_context_group();
        } else {
            close_context();
        }
        break;
    default:
        // THE KEY THAT OPENED IT CLOSES IT -- the shared rule, following the
        // opener's effective binding wherever a maker moved it.
        if (session_.keymap.matches(Act::kContextOpen, k.scancode, k.modifiers)) {
            close_context();
        }
        break;
    }
}

void WorkshopWeave::leave_context_group() {
    ContextMenu& menu = session_.context;
    const std::string was = menu.group;
    menu.group.clear();
    menu.cursor = 0;
    const std::vector<ContextEntry> rows = context_population(menu.subject, menu.group);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].is_group && was == rows[i].group) {
            menu.cursor = i;
            break;
        }
    }
}

// WL-CTX-08 -- agents/workshop/contextual.md
void WorkshopWeave::choose_context_row(loom::Mail& mail) {
    ContextMenu& menu = session_.context;
    const std::vector<ContextEntry> rows = context_population(menu.subject, menu.group);
    if (menu.cursor >= rows.size()) {
        return; // the belt, not the door
    }
    const ContextEntry chosen = rows[menu.cursor];
    if (chosen.is_group) {
        menu.group = chosen.group;
        menu.cursor = 0;
        return;
    }
    if (chosen.row == nullptr) {
        return; // unreachable: the catalog cross-check is a compile-time assertion
    }
    const ContextMenu spent = menu;
    close_context();
    spend_context_choice(chosen.row->act, spent, mail);
}

// WL-CTX-01, WL-CTX-02, WL-CTX-07, WL-CTX-08 -- agents/workshop/contextual.md
void WorkshopWeave::spend_context_choice(Act a, const ContextMenu& spent, loom::Mail& mail) {
    switch (a) {
    // -- the pointed pane ---------------------------------------------------------
    case Act::kArrange: enter_arrange_pane(spent.pane); break;
    case Act::kManageFront:
    case Act::kManageBack:
    case Act::kManageRaise:
    case Act::kManageLower:
    case Act::kManageResetPlace:
    case Act::kManageResetWidth:
    case Act::kManageResetHeight:
    case Act::kManageRemove: spend_pane_action(a, spent.pane, mail); break;
    // -- the pointed object -------------------------------------------------------
    case Act::kObjectDelete: context_delete_object(spent.object); break;
    // -- the pointed LAYOUT TAB ---------------------------------------------------
    //
    // EVERY ONE OF THESE TAKES THE CAPTURED POSITION and none of them switches first.
    // The subject is the tab the press named; the owner re-asks the run about it at
    // spend, exactly as the pane rows re-ask about a `PaneRef`, so a run that changed
    // while the menu was open refuses rather than acting on whoever moved into that
    // slot.
    case Act::kLayoutRename: open_layout_rename(spent.layout); break;
    case Act::kLayoutDuplicate: duplicate_layout(spent.layout, mail); break;
    case Act::kLayoutMoveLeft: shift_layout(spent.layout, -1); break;
    case Act::kLayoutMoveRight: shift_layout(spent.layout, +1); break;
    case Act::kLayoutRemove: drop_layout(spent.layout, mail); break;
    // -- the room -----------------------------------------------------------------
    case Act::kObjectNew: create_object(); break;
    case Act::kPicker: open_picker(); break;
    case Act::kArrangeDesk: open_arrange_desk(); break;
    case Act::kTerminalToggle: toggle_terminal(); break;
    case Act::kAttention: toggle_attention(); break;
    case Act::kHotkeys: toggle_hotkeys(); break;
    case Act::kSaveDocument: save_document(); break;
    case Act::kOpenDocument: load_document(); break;
    case Act::kSetupSave: save_setup(); break;
    case Act::kSetupRestore: restore_setup(mail); break;
    case Act::kManageResetOrder: reset_front_order(); break;
    default: break;
    }
}

// WL-CTX-08 -- agents/workshop/contextual.md
void WorkshopWeave::context_press(const PointedAt& at, std::int64_t space, std::int64_t x,
                                  std::int64_t y, loom::Mail& mail) {
    const ContextPressAt hit =
        context_press_at(session_, screen_of(session_), space, x, y, at);
    if (!hit.inside) {
        close_context();
        return;
    }
    if (!hit.entry) {
        return;
    }
    session_.context.cursor = hit.index;
    choose_context_row(mail);
}

// WL-TEXT-08 -- agents/workshop/text-box.md
void WorkshopWeave::on(const zengine::surface::ClipboardCopy& c, loom::Mail&) {
    session_.clipboard.text = c.text;
}

// WL-TEXT-09, WL-TEXT-10 -- agents/workshop/text-box.md
void WorkshopWeave::on(const zengine::surface::ClipboardText& a, loom::Mail& mail) {
    if (!mail.answers_ask()) {
        return; // not Loom's answer to anything this weave asked
    }
    const std::optional<loom::PendingAsk> settled =
        paste_asks_.settle(mail.correlation(), mail.sender());
    if (!settled) {
        return; // not a conversation this weave is waiting on
    }
    const PendingPaste p = take_pending_paste(settled->id);
    component::TextBox* box = nullptr;
    Row* row = nullptr;
    switch (p.owner) {
    case PasteOwner::kNone: return;
    case PasteOwner::kEditor: {
        // THE EDITOR'S SETTLEMENT PINS THE WHOLE POSITION, not just the draft: the
        // request recorded which document was open and exactly where it stood, and
        // the answer applies there or nowhere. A replaced or closed document strands
        // the payload silently (the dead draft's own fate); a document that MOVED --
        // any edit, any caret or selection change between request and answer -- gets
        // a sentence instead of a paste, because relocating the text to wherever the
        // caret is now would be answering a question the maker no longer asked.
        EditorState& e = session_.editor;
        if (!e.open_document() || e.doc_epoch != p.editor_doc) {
            return; // the document that asked is gone; discarded, silently
        }
        if (e.buffer.revision() != p.editor_revision) {
            say("the paste answer arrived after the source moved -- nothing was "
                "pasted; paste again",
                true);
            repaint(mail);
            return;
        }
        if (a.readable) {
            session_.clipboard.text = a.text; // the platform's current truth, asked for
        }
        if (session_.clipboard.text.empty()) {
            repaint(mail);
            return; // an empty clipboard pastes nothing, the component's own law
        }
        const PasteableSource judged = pasteable_source(session_.clipboard.text);
        if (!judged.representable) {
            say("the clipboard holds bytes outside plain ASCII, which this editor "
                "cannot carry truthfully -- nothing was pasted",
                true);
            repaint(mail);
            return;
        }
        e.buffer.paste_lines(judged.lines);
        e.follow_caret = true;
        repaint(mail);
        return;
    }
    case PasteOwner::kTerminal:
        // The line outlives the pane's visibility (shift+space hides it and keeps the
        // draft), so an open pane is not required — the same DRAFT is.
        box = &session_.terminal.input;
        break;
    case PasteOwner::kNaming:
        box = naming_line();
        break;
    case PasteOwner::kDraft:
        row = editing_row();
        if (row == nullptr || row->label() != p.label || session_.selected != p.object ||
            row->editor().draft_epoch() != p.epoch) {
            row = nullptr; // a different draft is standing (or none); not this paste's
        }
        break;
    }
    if (row != nullptr) {
        if (a.readable) {
            session_.clipboard.text = a.text; // the platform's current truth, asked for
        }
        row->paste(session_.clipboard);
    } else if (box != nullptr && box->draft_epoch() == p.epoch) {
        if (a.readable) {
            session_.clipboard.text = a.text;
        }
        box->paste(session_.clipboard);
        if (p.owner == PasteOwner::kTerminal) {
            refresh_terminal();
        }
    } else {
        return; // the draft that asked is gone; the payload is discarded, silently
    }
    repaint(mail);
}

// WL-KEY-03 -- agents/workshop/keyboard.md
void WorkshopWeave::on(const zengine::input::TextEntered& t, loom::Mail& mail) {
    if (!swallow_text_.empty()) {
        const std::string owed = swallow_text_;
        swallow_text_.clear();
        if (same_keystroke(t.text, owed)) {
            return; // the character the trigger produced belongs to the trigger
        }
    }
    if (t.text.empty()) {
        return;
    }
    // THE HOTKEY VIEW TAKES NO TEXT AND TYPES NONE, exactly as it spends the keys: a
    // maker reading a key list is not typing anywhere, and the surface beneath must
    // come back untouched when the view closes.
    if (session_.hotkeys.open) {
        return;
    }
    // WHERE A CHARACTER GOES IS THE SAME QUESTION AS WHERE A KEY GOES, and since
    // the keymap it is answered by the same resolver instead of by this function's own
    // hand-copy of the chain (the second of the five spellings the research measured).
    // Per branch, the standing law is unchanged: a mode that owns the keyboard whole
    // takes the text or deliberately types none (arrangement and the picker are driven
    // by unmodified letters, so every character produced while they are open belongs
    // to a gesture); a focused pane receives the text in exactly the position it
    // receives the keys -- the half that makes `%` reach a provider at all, since
    // Workshop maps no key to any character; a live draft types; and in command mode
    // text is simply not a command.
    switch (keyboard_context(session_)) {
    case KeyContext::kNaming:
        session_.setup.naming.line.type(t.text);
        repaint(mail);
        return;
    case KeyContext::kPaneNaming:
        session_.pane_naming.line.type(t.text);
        repaint(mail);
        return;
    case KeyContext::kTerminal:
        // AT THE CARET, WHICH IS NOT ALWAYS THE END. `type` is the only
        // door that moves the text and the caret together, so a keystroke in the
        // middle of a line cannot leave one behind. The line changed, so what could
        // be said next changed with it: typing IS the completion gesture.
        session_.terminal.input.type(t.text);
        refresh_terminal();
        repaint(mail);
        return;
    case KeyContext::kArrangePane:
    case KeyContext::kArrangeDesk:
    case KeyContext::kArrangeReset:
    case KeyContext::kPicker:
        return;
    case KeyContext::kPane:
        external_text(keyboard_pane(), t, mail);
        return;
    case KeyContext::kEditor:
        editor_text(t.text);
        repaint(mail);
        return;
    case KeyContext::kDraft: {
        Row* row = editing_row();
        if (row == nullptr) {
            return; // unreachable while the resolver holds; written anyway
        }
        row->type(t.text);
        repaint(mail);
        return;
    }
    default:
        return; // command mode: text is simply not a command
    }
}

// WL-ARR-02 -- agents/workshop/arrangement.md; WL-TAB-11 -- agents/workshop/tab-run.md
WorkshopWeave::GesturesEnded WorkshopWeave::end_held_gestures() {
    GesturesEnded out;
    if (session_.drag.active) {
        out.document = true;
        out.document_id = session_.drag.id;
        end_drag(session_);
    }
    if (session_.pane_drag.active) {
        out.pane_held = true;
        out.pane = session_.pane_drag.pane;
        session_.pane_drag = PaneGesture{};
    }
    // The text-selection drag ends silently and is not reported: the selection it swept
    // is on screen, which is the whole statement. The selection itself SURVIVES
    // the release — ending the sweep is not unselecting — so only the gesture record is
    // cleared here.
    session_.text_drag = TextDrag{};
    //...and so does the tab drag. The run's new order is on screen and the
    // moves were already narrated one step at a time, so a release has nothing to add;
    // what it must do is end the gesture, wherever the hand happens to be, for this
    // function's whole stated reason.
    session_.tab_drag = LayoutTabDrag{};
    return out;
}

// WL-FOCUS-03 -- agents/workshop/focus.md; WL-PRESS-04 -- agents/workshop/press-chain.md
void WorkshopWeave::on(const zengine::input::PointerButton& b, loom::Mail& mail) {
    // WHILE THE OVERLAY IS OPEN THE WORKSPACE GETS NOTHING, and that half is
    // unchanged: the pane covers the bottom-right of the screen,
    // workspace included, so a press there would take hold of an object the
    // maker cannot see -- and a press just outside it would move the document
    // out from under a mode they are typing in. One sentence covers both:
    // while the terminal is open, the terminal has the input. There is no
    // focus object, no capture and no z-order; closing it restores every
    // gesture exactly.
    //
    // WHAT CHANGED IS THAT THE TERMINAL NOW DOES SOMETHING WITH IT --
    // and only inside itself. The mode is still a MODE: it takes every
    // pointer event anywhere, and `terminal_press` decides whether one of the
    // regions the Terminal OWNS wants it. A press that lands on none of them
    // is still consumed by the mode rather than falling through, which is the
    // whole of what stops a click on the pane's empty middle from selecting
    // an object behind it. That is the first PLACE-WITHIN-A-MODE this
    // application has, and it is a bounds test against the Terminal's own
    // regions rather than a widget registry: nothing below is an entity,
    // nothing has an identity, and closing the pane removes all of it because
    // there is nothing to remove.
    if (session_.terminal.open) {
        // A RELEASE STILL ENDS A DRAG THAT BEGAN ON THE WORKSPACE, and this is a
        // repair rather than a new rule (own: "a gesture that began on the
        // workspace owns the pointer until it ends, so its release must end it
        // wherever the maker's hand happens to be"). Opening the pane mid-drag used
        // to swallow the release, leaving `drag.active` true with the button up --
        // after which closing the pane made the next bare motion drag an object
        // nobody was holding. Occluding a release is the one thing this file already
        // knew not to do; the overlay was doing it by arriving first.
        //
        // SILENTLY, WHICH IS THE ONE PLACE THIS FILE'S "SAY SO" RULE DOES NOT APPLY.
        // `toggle_terminal` already wrote down why: the notice line is not painted
        // while the pane covers it, so a sentence made now is one nobody can read and
        // that would then reappear, stale, when the pane closes -- and closing writes
        // its own notice over it anyway. The gesture is ended; there is nobody to
        // tell.
        if (!b.pressed && b.button == 1) {
            // EVERY BUTTON-1 GESTURE, and not only the document's. A pane
            // move or size begun in pane management is held in a second record, and
            // the overlay used to swallow its release exactly as it once swallowed the
            // document's: `pane_drag.active` stayed true with the button up, and the
            // first bare motion after the pane closed moved a window nobody was
            // holding. Measured -- the pane walked to the pointer.
            (void)end_held_gestures();
            return;
        }
        // AND THE BOOL BELOW IS NOT THE PRESS-CHAIN'S BOOL, which is why it is given a
        // name here. The three handlers under `if (b.pressed)` answer whether they
        // CONSUMED the press, and the chain stops on a true. `terminal_press` answers
        // whether anything CHANGED, and the answer does not decide anything about routing:
        // the mode consumed the press the moment `session_.terminal.open` was true, three
        // lines up, and a press that lands on none of the pane's regions is consumed there
        // just the same. Two different questions, two bools, and unifying them would put a
        // repaint decision on the routing path or a routing decision on the repaint path.
        if (b.pressed && b.button == 1) {
            const bool repaint_needed = terminal_press(b);
            if (repaint_needed) {
                repaint(mail);
            }
        }
        return;
    }
    // ARRANGEMENT IS A MODE AND IT OWNS THE POINTER WHILE IT IS OPEN -- the
    // Terminal's own shape, four lines up, for the same reason. While a maker is
    // arranging, every press is about a pane: letting one fall through to the
    // document would begin a drag on an object underneath a pane they are looking at,
    // which is the defect occupancy removed from panels in the first place.
    //
    // A SECONDARY PRESS IS THIS STATE'S WAY BACK OUT. The active interaction
    // that can truthfully interpret a secondary press receives first refusal, and
    // leaving is what this one truthfully means by it: the press leaves the
    // arrangement -- whichever scope, the reset prompt included -- and is CONSUMED
    // WHOLE. One consumed gesture performs one interaction transition: no context
    // menu opens from this press, and its release falls to the ordinary path's
    // non-primary drop exactly as every second-button release always has. This is a
    // state-local reading, not a Back command: there is no `right_click_back` action,
    // no keymap row, and the ordinary contextual opener still answers only the
    // presses no active interaction claimed.
    //
    // A RELEASE STILL ENDS A DOCUMENT DRAG THAT BEGAN BEFORE THE MODE DID, and this is
    // the same repair made for the pane: entering a mode mid-drag must not swallow
    // the release, or `drag.active` stays true with the button up and the next bare
    // motion drags an object nobody is holding.
    if (session_.arrange.open) {
        const PointedAt where = canvas_point_of(b.space, b.x, b.y);
        if (b.pressed && b.button == 3) {
            close_arrange();
            repaint(mail);
            return;
        }
        if (b.button != 1) {
            return;
        }
        if (!b.pressed) {
            const GesturesEnded done = end_held_gestures();
            if (done.pane_held) {
                // A RELEASE ENDS THE GESTURE WHEREVER THE HAND LANDS, and it is not asked
                // where that is -- the position is not part of ending something.
                say("placed " + ref_text(done.pane) + " -- " + arrange_status(), false);
                repaint(mail);
            }
            return;
        }
        if (!where.understood) {
            return;
        }
        arrange_press(where);
        repaint(mail);
        return;
    }
    // THE CONTEXTUAL SURFACE HAS FIRST REFUSAL WHILE IT IS OPEN -- a mode in
    // the two above's family, below both because both existed first and neither can
    // be open at the same time as this one through any current door. A press inside
    // it navigates or chooses; a press outside it dismisses and is CONSUMED, so a
    // click spent on closing a menu cannot also select an object, focus a pane or
    // reach a provider. A further right press re-asks the question about whatever is
    // pointed at now -- opening is re-targeting, not a toggle.
    if (session_.context.open) {
        const PointedAt where = canvas_point_of(b.space, b.x, b.y);
        if (b.pressed && b.button == 3) {
            if (where.understood) {
                open_context_at(where);
                repaint(mail);
            }
            return;
        }
        if (b.button != 1) {
            return;
        }
        if (!b.pressed) {
            // A RELEASE STILL ENDS A GESTURE THAT BEGAN BEFORE THE SURFACE OPENED --
            // the Terminal's and management's own repair: a right press can arrive
            // mid-drag, and occluding the release would leave a drag active with the
            // button up.
            (void)end_held_gestures();
            return;
        }
        if (!where.understood) {
            return;
        }
        context_press(where, b.space, b.x, b.y, mail);
        repaint(mail);
        return;
    }
    const PointedAt at = canvas_point_of(b.space, b.x, b.y);
    // A RIGHT PRESS ASKS "WHAT CAN I DO WITH THIS?". Before this branch a
    // second button meant nothing anywhere in Workshop, so consuming it displaces no
    // behaviour and steals nothing from any provider -- the pane seam cannot say a
    // second button, deliberately, and no `PanePressed` is sent for one. Only a press
    // opens; a release of button 3 falls through to the gate below and is dropped, as
    // every non-primary transition always was.
    if (b.pressed && b.button == 3 && at.understood) {
        //...AND A TAB IS A SUBJECT IT CAN NAME -- BEHIND OCCUPANCY.
        // The tab inverse is asked only once the ordinary walk has answered that the
        // Layouts pane owns this point, so the menu's subject is the tab under the hand
        // when the tabs are what is under the hand, and is whatever pane a maker put in
        // FRONT of them when it is not. Until this phase the question was asked first
        // and globally, which made a covered tab nameable through the pane covering it.
        // A right press on the create affordance names the room, not a layout: `+` is
        // an action rather than a thing, so there is nothing to ask about it.
        const Occupancy owner =
            occupied_at(session_.panels, session_.setup.active, screen_of(session_), at);
        const LayoutTabPress tab =
            owner.occupied && owner.kind == panel::kLayouts
                ? band_tab_at(session_, screen_of(session_), b.space, b.x, b.y)
                : LayoutTabPress{};
        if (tab.hit && !tab.create) {
            open_context_on_layout(at, tab.at);
        } else {
            open_context_at(at);
        }
        repaint(mail);
        return;
    }
    if (b.button != 1 || !at.understood) {
        return;
    }
    if (b.pressed) {
        // TRUE MEANS CONSUMED: STOP ROUTING. FALSE MEANS NOT CONSUMED: CARRY ON.
        // That is the whole meaning of the three bools below, and it is the only meaning
        // any of them has -- not "something changed", not "the act succeeded", not "the
        // press was accepted". A layer that consumes may refuse in its own words, may say
        // nothing at all, and may leave every fact in this application exactly as it found
        // it; what it may not do is let a press it owns be answered by the layer around
        // it. A consumed press does not have to change anything -- it only has to have
        // reached the layer that owns what the press means.
        //
        // AND THE BODY IS RESOLVED ONCE, HERE, beside the canvas point above it.
        // The three handlers under it are three questions about ONE place, and they used
        // to resolve it separately -- the same six lines three times, and up to three
        // resolutions of one body for one press. Holding it across the chain is safe for a
        // reason worth writing down rather than assuming: every one of the three changes
        // nothing on the paths where it declines, so a handler that says "not mine" has
        // not moved the picture the next handler is about to ask about.
        const InfoBodyAt where = info_body_at(state_, session_, b.space, b.x, b.y);
        // AND THE OCCUPANCY WALK IS RESOLVED HERE TOO beside the body and
        // the canvas point, for the reason the body was hoisted: it is one question
        // about one place, every handler below changes nothing on the path where it
        // declines, and the answer is now needed BEFORE the chain rather than after it.
        // It is the same pure walk `occupied_at` always was -- the picker first, then
        // the panes topmost-first, then nothing -- moved, not changed.
        const Occupancy here =
            occupied_at(session_.panels, session_.setup.active, screen_of(session_), at);
        // WHERE THE KEYBOARD GOES IS DECIDED BY THE PRESS ITSELF, IN ONE LINE, BEFORE
        // ANY LAYER ANSWERS IT. Putting it in the routing arms instead would be
        // four decisions -- one per arm, one of them easy to forget -- about a single
        // fact: which presentation did the maker just point at. A press on an external
        // pane points the keyboard there; a press on Workshop's own furniture, on the
        // workspace, or on nothing at all takes it away again.
        //
        // IT IS SET FOR THE WHOLE RECTANGLE, not for the rows inside it. A press on the
        // pane's header or on the padding under its last prose line names no row and
        // sends no `PanePressed` -- and it is still unambiguously a maker pointing at
        // that pane, which is the only question this line asks.
        //
        // AND THE MODES ABOVE NEVER REACH IT. The Terminal and pane management take
        // every press whole, one branch up, so opening either leaves the candidate
        // exactly where it was and closing it hands the keyboard straight back --
        // which is the same "closing it restores every gesture exactly" this file
        // already promises about the pointer.
        //
        // A BUILT-IN CAN BE A CANDIDATE WHEN ITS CATALOG ROW SAYS SO. The Editor was
        // the first -- a body a maker types into, so a press there points the keys at
        // their source exactly as a press into an external pane points them at a
        // provider -- and Project Files is the second, a list with a cursor and
        // gestures of its own. At two, the distinction stopped being something this
        // line should know: it is a fact about a KIND, so it is declared on the kind
        // (`PanelKind::takes_keyboard`) and read here. Every other built-in still
        // clears the candidate, and this is still not a focus framework -- one
        // declaration moved, nothing registered.
        //
        // ⚠ THE PRIOR ANSWER IS READ BEFORE IT IS OVERWRITTEN, because one arm below
        // needs it: Project Files activates a row only when the pane ALREADY had the
        // keys, and this line is what makes that untrue a moment later. Reading it
        // afterwards would make every first press look like a press in a pane the
        // maker was already working in.
        const bool files_had_keyboard = files_has_keyboard(session_);
        // WHICH PANE THE MAKER JUST POINTED AT -- ONE READING, TWO FACTS.
        // Selection is the wider of the two and the keyboard candidate is DERIVED
        // from it through the declared candidacy, rather than the occupancy being
        // tested twice: two reads of one press is how the desk comes to think one
        // pane is in front while the keys go to another. A press that lands on the
        // workspace, on the picker, on the screen's own furniture or on nothing
        // clears both by these same two lines.
        session_.panels.selected = here.occupied ? here.kind : kNoPaneKind;
        session_.panels.keyboard =
            session_.panels.selected != kNoPaneKind &&
                    kind_takes_keyboard(session_.panels.selected)
                ? session_.panels.selected
                : kNoPaneKind;
        // A VISIBLE PANEL OCCUPIES POINTER SPACE, and this is the
        // whole of it: the press is asked what it landed on before the
        // document is asked anything, and a press that landed on a panel
        // never reaches `take_hold` -- so it cannot select, cannot begin a
        // move and cannot begin a resize, because all three are that one
        // call. The question names no kind and knows no coordinate; it is
        // the same `bounds_of` the painter used for the same panel.
        //
        // IT SAYS SO RATHER THAN GOING QUIET. Every other press writes the
        // notice line, so a press that changed nothing and said nothing
        // would leave the previous gesture's sentence sitting beside a
        // maker who has just done something else -- a stale statement,
        // which is the one thing this tool is arranged against. It is also
        // the only way a maker learns that the panel is a thing rather than
        // a picture, since `[ Build ]` is not clickable yet.
        //
        // (`here` was resolved at the top of this branch -- one walk, for
        // two questions that are about the same press.)
        // AND AN EXTERNAL PANE IS THE ONE PRESENTATION WHOSE PRESS GOES SOMEWHERE
        //. It is the SAME occupancy answer -- one geometry walk, one topmost
        // rule, the picker still first -- asked one further question: this cell belongs
        // to a pane Workshop did not compile, so the press is that provider's.
        //
        // CONSUMED EITHER WAY, AND DECIDED HERE RATHER THAN THERE. A pane that owns
        // visible room owns pointer refusal for that room, and the refusal is Workshop's
        // to make because Workshop is what knows the room exists. Nothing waits for the
        // provider: there is no reply shape, `external_press` sends and returns, and a
        // press that named no row of the body (the header, the padding under the last
        // prose line, the lattice's edge) is consumed exactly the same and simply
        // travels no further. That is split -- the synchronous half of the
        // question is geometry Workshop already holds, so `consumed` never crosses the
        // wire.
        //
        // AND WORKSHOP SAYS NOTHING, WHICH IS THE ONE PLACE THE RULE ABOVE INVERTS.
        // The sentence three lines up is TRUE of a built-in -- there really is nothing
        // under a Builder to take hold of -- and would be a claim about an OUTCOME here,
        // made before the outcome exists: what a press on a provider's row means is that
        // provider's vocabulary, the answer arrives later as ordinary content, and
        // Workshop cannot name either. So the statement is the pane's to make, in its
        // own rows, and this layer leaves the line alone rather than writing a sentence
        // it would have to guess (a refusal belongs to the deepest layer whose
        // vocabulary contains the reason -- and this one's does not).
        if (here.occupied && is_runtime_kind(here.kind)) {
            external_press(here.kind, b, mail);
        } else if (here.occupied && here.kind == panel::kEditor) {
            // A PRESS INTO THE EDITOR PLACES THE CARET AND BEGINS A SELECTION SWEEP
            // -- the draft's and the Terminal line's own press, over a document. It
            // says nothing: the caret is the statement, and the candidate line above
            // already pointed the keys here for the whole rectangle. A press on the
            // header or past the body's rows moves nothing and is consumed exactly
            // as an external pane's header press is.
            editor_press(b);
        } else if (here.occupied && here.kind == panel::kProjectFiles) {
            // AND A PRESS INTO THE PROJECT BROWSER SELECTS A ROW -- the editor's arm,
            // over a list. A press on the header or past the last row moves nothing
            // and is consumed exactly as the editor's is.
            files_press(b, files_had_keyboard, mail);
        } else if (here.occupied && here.kind == panel::kPaneEditor) {
            // AND A PRESS INTO THE PANE EDITOR -- Files' arm, one pane over:
            // a pane row chooses the SUBJECT, a field row moves the row cursor, the
            // live draft's own row places the caret, and the heading or the padding
            // is consumed as a focus statement. The selection line above has already
            // made this pane the selected one; nothing in here reads that fact.
            pane_editor_press(b, b.modifiers);
        } else if (here.occupied && here.kind == panel::kInfo &&
                   (info_press(where, b.modifiers) || actions_press(where) ||
                    objects_press(where))) {
            // THE INFO PANEL'S OWN THREE INVERSES, BEHIND THE OWNERSHIP DECISION LIKE
            // EVERY OTHER PANE'S. They are unchanged -- same order, same
            // disjointness, same `true means consumed` -- and what changed is only
            // WHERE they are asked. Until this phase all three ran BEFORE the occupancy
            // walk and never consulted the effective order, so a pane authored over the
            // side column and ranked in FRONT of Info still lost its presses on Info's
            // control cells to Info: see-here, press-there, at the same boundary the
            // top band had it. Nothing here is a new routing layer; three questions
            // moved down into the arm that already knew which pane owns the point.
            //
            // THE ACTIVE PROPERTY EDITOR IS ASKED FIRST, and it is a PLACE inside a
            // panel rather than a mode: the innermost thing that owns the
            // pointer where it landed answers before the thing around it, and a press
            // it declines falls through unchanged. It says nothing and consumes whether
            // or not the caret moved -- the caret IS the statement, and a sentence
            // repeating it would push off the line a refusal the maker may still need
            //.
            //
            // THEN THE ACTION CONTROLS, then the OBJECT LIST. The three
            // runs of the body cannot fight over a press -- the footer, the object list
            // and a live draft's own row are disjoint runs of ONE row budget, which is
            // what making the body one region bought -- so this ordering is
            // written down because an ordering resting on a disjointness proof is one
            // refactor from being silently wrong, not because two of them could answer.
            //
            // ⚠ THE SHORT CIRCUIT IS THE CHAIN, and it is exact: `||` stops at the
            // first `true`, and each of the three changes nothing on the path where it
            // declines -- which is the property that let the body be hoisted in the
            // first place. A press none of them owns falls to Info's own sentence
            // below, which is what it always did.
            repaint(mail);
            return;
        } else if (here.occupied && here.kind == panel::kLayouts &&
                   layouts_press(b, mail)) {
            // AND THE LAYOUTS PANE'S OWN INVERSE -- the tabs, `+`, the rename
            // second press and the reorder drag, asked ONLY once the ordinary walk has
            // said this point is that pane's. It is `files_press`' position exactly.
            // The inverse itself is still specialised to Layouts and still rule
            // end to end (the spans come from `band_status`' own composition); what is
            // gone is the coordinate exception that used to ask it first, above every
            // pane, from a rectangle nothing else could name.
            repaint(mail);
            return;
        } else if (here.occupied) {
            say(std::string(here.what) + " is here -- nothing under it can be taken hold of",
                false);
        } else {
            const std::int64_t id = take_hold(state_, session_, workspace_cell_x(at.cell.x),
                                              workspace_cell_y(at.cell.y));
            if (id != 0) {
                const bool sizing = session_.drag.resizing;
                select(id);
                say("holding #" + std::to_string(id) +
                        (sizing ? " -- drag to resize it" : " -- drag to move it"),
                    false);
            } else {
                say("nothing there", false);
            }
        }
    } else {
        // A RELEASE IS NOT ASKED THE SAME QUESTION, and the asymmetry is the
        // reason no capture state exists here. A gesture that began on the
        // workspace owns the pointer until it ends, so its release must end
        // it wherever the maker's hand happens to be -- occluding the
        // release would strand `drag.active` true with the button up, and
        // the next motion would drag an object nobody was holding. The
        // other direction needs nothing at all: a press on a panel starts no
        // drag, so a release after one finds none and does nothing at all.
        // The absence of a drag IS the memory.
        //
        // THROUGH THE SAME OWNER AS EVERY OTHER MODE, so there is one place
        // that knows what a button-1 release ends and three places that decide what to
        // SAY about it. No pane gesture can reach this branch today -- one is begun
        // only while management is open, which routes above -- and asking the owner
        // rather than a field is what keeps that a fact rather than an assumption.
        const GesturesEnded done = end_held_gestures();
        if (done.document) {
            say("released #" + std::to_string(done.document_id), false);
        }
    }
    repaint(mail);
}

// WL-PANE-05 -- agents/workshop/panes-and-windows.md
void WorkshopWeave::on(const zengine::input::PointerMoved& m, loom::Mail& mail) {
    // ---- READING PAST AN ELLIPSIS, BEFORE ANYTHING ELSE THIS MOTION MEANS ------------
    //
    // IT IS A POINTING AND NOT A GESTURE, which is why it is resolved here rather than in
    // one of the branches below: nothing is held, nothing is claimed, and the answer is a
    // pure function of where the pointer is right now (`reveal_for`) compared with what
    // the session was already showing. A motion that changes neither costs no repaint,
    // which is what keeps a hand crossing the screen from republishing the canvas.
    //
    // A MODE THAT OWNS THE POINTER OWNS THIS TOO. While the Terminal, an arrangement
    // scope or the contextual surface is open, a motion is theirs -- and so is the
    // absence of a reveal, because a row a maker cannot point at is not a row they are
    // pointing at.
    //
    // AND A HELD GESTURE IS NOT A HOVER. A hand sweeping a selection, moving an object or
    // sizing a pane is doing something with the pointer; scrolling a row underneath it
    // would be a second meaning for one motion.
    const bool pointer_is_spent = session_.terminal.open || session_.arrange.open ||
                                  session_.context.open || session_.hotkeys.open ||
                                  session_.attention.open || session_.text_drag.active ||
                                  session_.drag.active || session_.pane_drag.active ||
                                  session_.tab_drag.active;
    const Revealed want =
        pointer_is_spent ? Revealed{}
                         : reveal_for(state_, session_, m.space, m.x, m.y);
    if (!want.same_as(session_.reveal)) {
        session_.reveal = want;
        repaint(mail);
    }
    // ---- CARRYING A LAYOUT TAB ALONG THE RUN -----------------------------------------
    //
    // THE HAND IS HOLDING THE LIVE LAYOUT, because the press that began this made that
    // tab live. So a motion asks the same inverse the press asked -- against the run as
    // it is painted RIGHT NOW, which has already reordered under any earlier step of
    // this same drag -- and moves the live layout to whatever tab it is over.
    //
    // NOTHING IS CACHED AND NOTHING IS RECONCILED. `move_layout` changes order and only
    // order; no desk is replaced, so there is no `apply_setup` and no provider hears a
    // thing. A motion that is over no tab, over the create affordance or over the live
    // tab's own span moves nothing -- which is what makes dragging past the end of the
    // run rest rather than wrap.
    if (session_.tab_drag.active) {
        const LayoutTabPress over =
            band_tab_at(session_, screen_of(session_), m.space, m.x, m.y);
        if (over.hit && !over.create &&
            move_layout(session_.setup, session_.setup.active_at, over.at)) {
            repaint(mail);
        }
        return;
    }
    if (session_.terminal.open) {
        // THE OVERLAY HAS THE INPUT, and one motion matters inside it: a
        // selection drag the mode's own press began on its editable line. The geometry
        // is re-resolved from the CURRENT screen — the same two calls the press spent —
        // and the ROW is deliberately not re-tested: a drag owns the gesture until
        // release (`PaneGesture`'s law), so a hand that wanders off the line keeps
        // sweeping the line by column, which is what makes the selection stable rather
        // than flickering with the pointer's row (SC-form: stable across the region).
        if (session_.text_drag.active &&
            session_.text_drag.place == text_drag_place::kTerminalLine) {
            const Screen sc = screen_of(session_);
            const TerminalInputPlace place = terminal_input_place(sc);
            const ProseAt at =
                prose_at(m.space, m.x, m.y, place.region_x, place.region_y, place.fit);
            if (at.understood) {
                session_.terminal.input.drag_to_column(
                    terminal_value_column(place, at.column));
                refresh_terminal();
                repaint(mail);
            }
        }
        return;
    }
    // AND ARRANGEMENT OWNS MOTION WHILE IT IS OPEN, for the press's reason. A motion
    // with no pane gesture held does nothing at all: only a PRESS begins one, which is
    // the same sentence this handler already said about the document.
    if (session_.arrange.open) {
        const PointedAt here = canvas_point_of(m.space, m.x, m.y);
        if (!here.understood || !session_.pane_drag.active) {
            return;
        }
        arrange_motion(here.sub.x, here.sub.y, mail);
        repaint(mail);
        return;
    }
    // A SELECTION DRAG ON THE PANE EDITOR'S LIVE DRAFT -- the property draft's
    // twin below, resolved through the Pane Editor's own body.
    if (session_.text_drag.active &&
        session_.text_drag.place == text_drag_place::kPaneEditorDraft) {
        Row* row = pane_editor_editing_row();
        const PaneEditorAt where = pane_editor_at(session_, m.space, m.x, m.y);
        if (row != nullptr && where.present) {
            row->drag_to_column(property_value_column(where.at.column));
            refresh_inspector();
            repaint(mail);
        }
        return;
    }
    // A SELECTION DRAG ON THE LIVE PROPERTY DRAFT — the Terminal branch's twin
    // on the ordinary path, before the document's drag for the same reason the press
    // chain asks the draft first: it is the narrower claim, and the two cannot both be
    // active (a press `info_press` consumed never reached `take_hold`).
    if (session_.text_drag.active &&
        session_.text_drag.place == text_drag_place::kPropertyDraft) {
        Row* row = editing_row();
        const InfoBodyAt where = info_body_at(state_, session_, m.space, m.x, m.y);
        if (row != nullptr && where.present) {
            row->drag_to_column(property_value_column(where.at.column));
            refresh_inspector();
            repaint(mail);
        }
        return;
    }
    // A SELECTION DRAG IN THE SOURCE EDITOR — the third editable place, and the first
    // where the ROW is meaningful mid-drag: a document has many. The geometry is
    // re-resolved per motion through the same resolution the press spent; a hand past
    // the body's top or bottom edge steps the caret one row further per motion (the
    // component's leftward-step law turned vertical), and the follow flag then pulls
    // the viewport after it -- deterministic, minimal, and enough to sweep a
    // selection out of the window a motion at a time.
    if (session_.text_drag.active &&
        session_.text_drag.place == text_drag_place::kEditorBody &&
        session_.editor.open_document()) {
        const Screen sc = screen_of(session_);
        const ExternalBodyPlace body = editor_body(session_, sc);
        if (!body.present) {
            return;
        }
        const ProseAt at = prose_at(m.space, m.x, m.y, body.region_x, body.region_y,
                                    body.fit);
        if (!at.understood) {
            return;
        }
        EditorState& e = session_.editor;
        const std::int64_t brow = at.row - body.header_rows;
        std::size_t target;
        if (brow < 0) {
            target = e.first_row > 0 ? e.first_row - 1 : 0;
        } else if (brow >= body.rows) {
            target = e.first_row + static_cast<std::size_t>(body.rows);
        } else {
            target = e.first_row + static_cast<std::size_t>(brow);
        }
        e.buffer.drag_to(target,
                         at.column < 0 ? std::int64_t{-1} : e.first_col + at.column);
        e.follow_caret = true;
        repaint(mail);
        return;
    }
    const PointedAt at = canvas_point_of(m.space, m.x, m.y);
    if (!at.understood || !session_.drag.active) {
        return;
    }
    const bool sizing = session_.drag.resizing;
    const Handled done = drag_to(state_, session_, workspace_cell_x(at.cell.x),
                                 workspace_cell_y(at.cell.y));
    if (!done.accepted()) {
        // A gesture can still propose something the document refuses, and it
        // must say so rather than have the setter quietly correct it. What it
        // may do -- and does -- is STOP at a boundary before proposing; that
        // is a different event, reported below in different words, with the
        // object actually changed.
        say(done.written.refusal, true);
    } else {
        const ui::Element* e = doc::find(state_, session_.drag.id);
        if (e != nullptr) {
            say(sizing ? size_notice(*e, done) : move_notice(*e, done), false);
        }
    }
    repaint(mail);
}

// WL-EDIT-10 -- agents/workshop/editor.md
void WorkshopWeave::on(const zengine::input::PointerWheel& w, loom::Mail& mail) {
    if (session_.terminal.open || session_.arrange.open || session_.context.open) {
        return;
    }
    const Screen sc = screen_of(session_);
    const PointedAt at = canvas_point_of(w.space, w.x, w.y);
    if (!at.understood) {
        return;
    }
    // The TOPMOST presentation under the wheel decides -- a pane in front owns its
    // own cells, and scrolling something under somebody else's pane is the
    // imaginary-reach this test refuses. The picker answers first inside the walk,
    // exactly as it does for a press, and it is the one occupant with no kind.
    const Occupancy here =
        occupied_at(session_.panels, session_.setup.active, sc, at);
    if (!here.occupied) {
        return;
    }
    if (here.kind == kNoKind) {
        picker_wheel(w, mail);
        return;
    }
    if (is_runtime_kind(here.kind)) {
        external_wheel(here.kind, w, mail);
        return;
    }
    if (here.kind == panel::kProjectFiles) {
        files_wheel(w, sc, mail);
        return;
    }
    if (here.kind == panel::kPaneEditor) {
        pane_editor_wheel(w, mail);
        return;
    }
    if (here.kind != panel::kEditor || !session_.editor.open_document()) {
        return;
    }
    if (!over_editor_body(session_, sc, w.space, w.x, w.y)) {
        return;
    }
    EditorState& e = session_.editor;
    const std::int64_t lines = spend_wheel(e.wheel_accum, w.dy, kEditorWheelLines);
    if (lines == 0) {
        return;
    }
    const ExternalBodyPlace body = editor_body(session_, sc);
    if (!body.present) {
        return;
    }
    const std::size_t rows = static_cast<std::size_t>(body.rows);
    const std::size_t total = e.buffer.line_count();
    const std::size_t furthest = total > rows ? total - rows : 0;
    std::size_t first = e.first_row;
    if (lines > 0) {
        const std::size_t up = static_cast<std::size_t>(lines);
        first = first > up ? first - up : 0;
    } else {
        first += static_cast<std::size_t>(-lines);
    }
    if (first > furthest) {
        first = furthest;
    }
    if (first == e.first_row) {
        return; // already at the edge: nothing moved, nothing repaints
    }
    e.first_row = first;
    repaint(mail);
}

// WL-PROJ-11, WL-PROJ-12 -- agents/workshop/project.md
void WorkshopWeave::on(const zengine::builder::BuildStatus& said, loom::Mail& mail) {
    if (!session_.panels.has(panel::kBuilder)) {
        return;
    }
    BuilderPane& pane = session_.panels.builder;
    // WAS THIS PANEL WATCHING? The first live run got this wrong and the
    // screen said so: reopening the panel asks the tool, the tool answers
    // with the outcome of a build that finished a minute ago, and the notice
    // line announced `built zengine-snake -- exit 0` as though it had just
    // happened. LEARNING a fact and WITNESSING an event are different, and
    // only the second is news. So the announcement below is made only for a
    // build this panel asked for and has not yet been answered about.
    //
    // THE ASYNC BUILD MADE THAT DISTINCTION WORTH MORE, NOT LESS. A build now has a
    // middle, so a panel opened while one is running is TOLD "running" and
    // must announce nothing -- it did not watch this build begin, and the
    // arrival of a status is not the arrival of an event. `awaiting` is
    // therefore held across every intermediate condition and released only
    // when the build reaches one it will not leave: `still_going` is the one
    // place that list is written down.
    const bool watching = pane.awaiting;
    pane.heard = true;
    pane.shown = said;
    if (!zengine::builder::still_going(said.outcome)) {
        pane.awaiting = false;
    }
    // ---- THE SECOND ANSWER, ANNOUNCED ON ITS OWN LATCH --------------------
    //
    // Realization settles AFTER the build it followed, so by the time it does,
    // `awaiting` has already been released and the build's own ending announced.
    // This is the other half of the same discipline: a panel that WATCHED a
    // realization begin may report how it ended, and a panel that merely learned
    // about one may not.
    //
    // ⚠ WHEN BOTH SETTLE IN ONE ARRIVAL, THE BUILD'S SENTENCE WINS. A failed build
    // refuses realization in the same breath, and there is one notice line: the
    // maker needs the CAUSE ("BUILD FAILED ... exit 1"), not the consequence
    // ("nothing was offered to the project"), which the panel's own rows carry
    // anyway. The latch is still released, so the derivative refusal is not
    // announced late as though it were news.
    const bool build_news = watching && !zengine::builder::still_going(said.outcome);
    const bool realization_settled =
        pane.awaiting_realization &&
        (said.realization == zengine::builder::realization::kRealized ||
         said.realization == zengine::builder::realization::kRefused);
    if (realization_settled) {
        pane.awaiting_realization = false;
        if (!build_news) {
            if (said.realization == zengine::builder::realization::kRealized) {
                say("realized " + said.artifact + " -- " + said.realized_detail, false);
            } else {
                say("NOT REALIZED: " + said.artifact + " -- " + said.realized_detail, true);
            }
            repaint(mail);
            return;
        }
    }
    if (!build_news) {
        repaint(mail);
        return;
    }
    // A FINISHED BUILD IS THE ONE THING THIS APPLICATION ALREADY KNOWS ABOUT THAT
    // CHANGES THE PROJECT ON DISK, so it is the one message that earns the browser a
    // fresh listing -- which is how Project Files stays truthful with no watcher, no
    // poll and no timer, and without one byte added to any protocol.
    //
    // IT IS GATED ON `build_news` AND NOT ON THE ARRIVAL. The tool republishes its
    // whole picture on every transition and again whenever a panel opens, so
    // "a status arrived" is not "a build finished": scanning on arrival would walk
    // the directory for a build that ended before this pane existed. `build_news` is
    // the fact this weave already derives for exactly that distinction -- a build
    // this session watched, which has now reached an outcome it will not leave.
    files_build_settled();
    switch (said.outcome) {
    case zengine::builder::outcome::kSucceeded:
        // TWO OUTCOMES, TWO SENTENCES, AND THE SECOND IS NOT SUPPRESSED BY THE
        // FIRST. A maker who asked for BUILD & REALIZE and got a green
        // build has learned half of what they asked about; announcing only that
        // half would be the same conflation the Builder's own two fields exist to
        // prevent. The realization half arrives later, in its own status, and is
        // announced by `on(BuildStatus)` reaching this switch again -- which it
        // does, because a realization answer republishes the tool's picture.
        say("built " + said.artifact + " -- exit 0", false);
        break;
    case zengine::builder::outcome::kFailed:
        say("BUILD FAILED: " + said.recipe + " -- exit " + std::to_string(said.status), true);
        break;
    case zengine::builder::outcome::kNoArtifact:
        say("the build succeeded and produced no `" + said.artifact + "`: " + said.detail,
            true);
        break;
    case zengine::builder::outcome::kNotStarted:
        say("the build never started: " + said.detail, true);
        break;
    case zengine::builder::outcome::kUnknownRecipe:
        say("the Builder refused: " + said.detail, true);
        break;
    default: break;
    }
    repaint(mail);
}

// WL-PROJ-07 -- agents/workshop/project.md
void WorkshopWeave::on(const zengine::builder::RecipeCatalog& said, loom::Mail& mail) {
    if (!session_.panels.has(panel::kBuilder)) {
        return;
    }
    BuilderPane& pane = session_.panels.builder;
    // THE IDENTITY IS TAKEN BEFORE THE CATALOG MOVES, because it is the only thing
    // about the old catalog worth carrying across. An empty string is the honest
    // absence -- nothing was selected, or nothing was there to select.
    const std::string was = pane.chosen < pane.known.recipes.size()
                                ? pane.known.recipes[pane.chosen].recipe
                                : std::string();
    pane.known = said;
    std::size_t now = pane.known.recipes.size(); // = not found
    for (std::size_t i = 0; i < pane.known.recipes.size(); ++i) {
        if (!was.empty() && pane.known.recipes[i].recipe == was) {
            now = i;
            break;
        }
    }
    if (now < pane.known.recipes.size()) {
        // THE SAME RECIPE, WHEREVER IT NOW SITS. `picked` is untouched: whether this
        // selection was the maker's explicit act or the catalog's own first row is a
        // fact about how it was made, and a reordering does not change it.
        pane.chosen = now;
        repaint(mail);
        return;
    }
    pane.chosen = 0;
    // A SELECTION THAT NO LONGER NAMES ANYTHING IS NOT THE MAKER'S ANY MORE:
    // the recipe their pick named is gone, and 0 is where the panel put them, not
    // where they went. The frontier action must not read it as an explicit choice.
    pane.picked = false;
    repaint(mail);
}

} // namespace zengine::workshop
