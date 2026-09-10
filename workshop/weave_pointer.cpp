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
    // ⭐ THE EDITOR'S SETTLEMENT ARM WAS HERE AND IS GONE (VD-25). It pinned the answer to
    // the document AND the position the ask recorded -- a moved buffer got a sentence, a
    // replaced one silence -- and that whole discipline moved with the buffer: the Editor
    // weave asks the Skin itself and judges the answer against its own epoch and revision
    // (`editor-pane/pane.cpp`).
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
    } else {
        return; // the draft that asked is gone; the payload is discarded, silently
    }
    repaint(mail);
}

// WL-KEY-03 -- agents/workshop/keyboard.md
void WorkshopWeave::on(const zengine::input::TextEntered& t, loom::Mail& mail) {
    if (quitting_) {
        HeldInput held;
        held.kind = HeldInput::Kind::kText;
        held.text = t;
        (void)hold_input(std::move(held));
        return;
    }
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
    case KeyContext::kArrangePane:
    case KeyContext::kArrangeDesk:
    case KeyContext::kArrangeReset:
    case KeyContext::kPicker:
        return;
    case KeyContext::kPane:
        external_text(keyboard_pane(), t, mail);
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
    if (quitting_) {
        HeldInput held;
        held.kind = HeldInput::Kind::kButton;
        held.button = b;
        (void)hold_input(std::move(held));
        return;
    }
    // ⭐ THE TERMINAL'S MODAL BRANCH WAS HERE AND IS GONE (VD-24). While the overlay was
    // open it took every pointer event anywhere -- a press outside its own regions was
    // consumed rather than falling through -- because it was drawn over the room with no
    // boundary and a press "just outside it" had no honest owner. A pane has a boundary by
    // construction: a press inside it is the pane's through `external_press_row` like every
    // other pane's, and a press outside it belongs to whatever is there. The release repair
    // this branch carried (`end_held_gestures` on a button-1 release, so opening the overlay
    // mid-drag could not strand the gesture) is not needed for a pane, because a pane does
    // not arrive over a drag in progress.
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
        // AND THE OCCUPANCY WALK IS RESOLVED HERE, beside the canvas point above it: it is
        // one question
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
        // the first -- a body a maker types into -- and Project Files the second, a list
        // with a cursor and gestures of its own; at two, the distinction stopped being
        // something this line should know: it is a fact about a KIND, so it is declared
        // on the kind (`PanelKind::takes_keyboard`) and read here. Both of those are
        // weaves now and take the keys as every runtime pane does; the declaration
        // stays on the kind for the built-ins that remain, and this is still not a
        // focus framework -- one declaration, nothing registered.
        //
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
            // AND A PRESS THAT NAMED A ROW OF THE PANE'S BODY TAKES HOLD OF THAT PANE FOR
            // THE LENGTH OF THE BUTTON. The record is this host's -- which pane, by handle,
            // and that a sweep is in progress -- and nothing else: what the sweep MEANS is
            // the pane's, told to it one `PaneDragged` per motion (`external_drag`), and
            // the release ends the record silently (`end_held_gestures`) and sends nothing,
            // because a pane resolves a sweep from the positions it was given and needs
            // no sentence saying the hand let go. A press on the header or the padding
            // begins no sweep: it named no row, so there is nothing for a motion to extend.
            if (external_press(here.kind, b, mail)) {
                session_.text_drag.active = true;
                session_.text_drag.place = text_drag_place::kExternalPane;
                session_.text_drag.kind = here.kind;
            }
        } else if (here.occupied && here.kind == panel::kPaneEditor) {
            // AND A PRESS INTO THE PANE EDITOR -- Files' arm, one pane over:
            // a pane row chooses the SUBJECT, a field row moves the row cursor, the
            // live draft's own row places the caret, and the heading or the padding
            // is consumed as a focus statement. The selection line above has already
            // made this pane the selected one; nothing in here reads that fact.
            pane_editor_press(b, b.modifiers);
        } else if (here.occupied && here.kind == panel::kLayouts &&
                   layouts_press(b, mail)) {
            // AND THE LAYOUTS PANE'S OWN INVERSE -- the tabs, `+`, the rename
            // second press and the reorder drag, asked ONLY once the ordinary walk has
            // said this point is that pane's -- every pane's own press position.
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
    if (quitting_) {
        HeldInput held;
        held.kind = HeldInput::Kind::kMoved;
        held.moved = m;
        (void)hold_input(std::move(held));
        return;
    }
    // ⭐ READING PAST AN ELLIPSIS WAS THE FIRST THING THIS HANDLER DID, AND IT LEFT WITH THE
    // INFO PANEL. A motion used to resolve `reveal_for` before anything else it might mean and
    // scroll a truncated row under the hand; that feature was Info's alone, needs the row's
    // unfitted text, and the text is the pane's now (`screen_reveal.cpp` says the rest). What
    // this handler does now is what it always did after that: carry a tab, sweep a selection,
    // move an object, size a pane.

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
    // ⭐ THE TERMINAL'S MOTION BRANCH WAS HERE AND IS GONE (VD-24), AND THE SOURCE
    // EDITOR'S WENT AFTER IT (VD-25) -- with it the last selection this host resolved
    // against a document of its own. A pane's own body is swept by the pane, out of the
    // presses and the motions it is sent; what this host resolves is the geometry, below.
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
    // ⭐ A SELECTION DRAG ON THE LIVE PROPERTY DRAFT LEFT WITH THE INFO PANEL. The draft is the
    // pane's own line now, inside the pane's own room, and a sweep across it is a press and a
    // motion the pane resolves against its own composition -- this host has no body to resolve
    // it against and no row to sweep.

    // A SELECTION SWEEP IN A PANE THIS HOST DID NOT COMPILE -- the source editor's own
    // arm, made general, and the one motion that crosses the seam. The press that began it
    // recorded the pane; each motion is resolved against that pane's body AS IT IS NOW,
    // through the same measurer the press spent, and crosses UNCLAMPED: a hand above the
    // body is a negative row, a hand below it a row past the granted count, and what either
    // means (step the viewport, extend the range, ignore it) is the pane's own vocabulary.
    // A pane that is no longer seated ends the sweep inside `external_drag`, with nothing
    // sent; nothing here repaints, because nothing here changed what is shown -- the
    // pane's next content will.
    if (session_.text_drag.active &&
        session_.text_drag.place == text_drag_place::kExternalPane) {
        external_drag(session_.text_drag.kind, m, mail);
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
    if (quitting_) {
        HeldInput held;
        held.kind = HeldInput::Kind::kWheel;
        held.wheel = w;
        (void)hold_input(std::move(held));
        return;
    }
    if (session_.arrange.open || session_.context.open) {
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
    if (here.kind == panel::kPaneEditor) {
        pane_editor_wheel(w, mail);
        return;
    }
    // ⭐ THE SOURCE EDITOR'S WHEEL ARM WAS HERE AND IS GONE (VD-25): the last wheel this
    // host spent on a viewport of its own. A pane's viewport is the pane's, and the wheel
    // reaches it as `PaneWheel` two arms up, like every other pane's.
}

// WL-PRESS-05 -- agents/workshop/press-chain.md; WL-TAB-09 -- agents/workshop/tab-run.md
bool WorkshopWeave::layouts_press(const zengine::input::PointerButton& b, loom::Mail& mail) {
    const LayoutTabPress tab =
        band_tab_at(session_, screen_of(session_), b.space, b.x, b.y);
    if (!tab.hit) {
        return false;
    }
    if (tab.create) {
        // THE `+` IS THE POINTER'S SPELLING OF `layout.new` AND NOTHING MORE:
        // the same door, the same ceiling, the same refusal in the same words. It arms
        // no double-click and begins no drag -- it is not a tab.
        session_.tab_click = TabClickMemory{};
        new_layout(mail);
        return true;
    }
    // A SECOND PRESS ON THE SAME TAB RENAMES IT, and the first one has already
    // made that tab live -- which is why the editor's subject and the switch cannot
    // disagree. `press_selects_word`'s discipline exactly: the completing press SPENDS
    // the arming, so there is no triple-click, and a first press is an ordinary switch
    // with an arming left beside it.
    const std::int64_t now = interaction_now();
    if (doubles_a_tab_click(session_.tab_click, tab.at, now)) {
        session_.tab_click = TabClickMemory{};
        open_layout_rename(tab.at);
        return true;
    }
    session_.tab_click = TabClickMemory{true, tab.at, now};
    // AND THE PRESS TAKES HOLD OF THE TAB. A press that becomes a drag
    // reorders; a press that does not is exactly the switch it always was, because a
    // drag that never moved lands the layout back where it started. The record holds no
    // position: the switch below has just made this tab the live one, so what is being
    // carried is always `setup.active_at`.
    session_.tab_drag.active = true;
    switch_layout(tab.at, mail);
    return true;
}

} // namespace zengine::workshop
