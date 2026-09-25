// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s contextual-action surface and the pointer.
// Workshop law: agents/workshop/contextual.md (+9 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- What can I do with this? The contextual-action surface ---------------

// WL-CTX-01 -- agents/workshop/contextual.md
void WorkshopWeave::open_context_at(const PointedAt& at) {
    ContextMenu next;
    next.open = true;
    // The press's own cell anchors the surface beside the hand, on both media at the cell grain.
    next.anchored = true;
    next.anchor_x = at.cell.x;
    next.anchor_y = at.cell.y;
    const Occupancy here =
        occupied_at(session_.panels, session_.setup.active, screen_of(session_), at);
    if (here.occupied) {
        // The setup row that resolves to the pointed presentation: the durable identity, never
        // the kind handle. A rectangle no row resolves to falls through to the room.
        for (const SetupPane& row : session_.setup.active.panes) {
            const std::optional<std::int64_t> named =
                resolve_pane(row.ref, session_.panels);
            if (named.has_value() && *named == here.kind) {
                next.subject = context_subject::kPane;
                next.pane = row.ref;
                break;
            }
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
    session_.context = next; // the room
}

void WorkshopWeave::close_context() { session_.context = ContextMenu{}; }

void WorkshopWeave::context_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    // A PANE'S MENU IS ITS PRESENTER'S TO NAVIGATE: the key is named by the same contextual rows
    // and forwarded (`menu_key`); only the host's own menu is walked here.
    if (session_.presented.open) {
        menu_key(k, mail);
        return;
    }
    ContextMenu& menu = session_.context;
    const std::vector<ContextEntry> rows = context_population(menu);
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
        // Escape does the smaller thing: out of an open group, else out of the surface.
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
    const std::vector<ContextEntry> rows = context_population(menu);
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
    const std::vector<ContextEntry> rows = context_population(menu);
    if (menu.cursor >= rows.size()) {
        return; // the belt, not the door
    }
    const ContextEntry chosen = rows[menu.cursor];
    // The host's own menu: every row it chooses is one of the host's own operations.
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
    // ...AND THE POINTED PANE'S CODE, through the same seam: the captured reference, never the
    // selection or the keyboard's pane, and nothing is selected, focused or seated on the way.
    case Act::kEditCode:
    case Act::kManageRemove: spend_pane_action(a, spent.pane, mail); break;
    // -- the pointed layout tab ---------------------------------------------------
    // Each takes the captured position and none switches first: the owner re-asks the run at
    // spend, so a run that changed while the menu was open refuses.
    case Act::kLayoutRename: open_layout_rename(spent.layout); break;
    case Act::kLayoutDuplicate: duplicate_layout(spent.layout, mail); break;
    case Act::kLayoutMoveLeft: shift_layout(spent.layout, -1); break;
    case Act::kLayoutMoveRight: shift_layout(spent.layout, +1); break;
    case Act::kLayoutRemove: drop_layout(spent.layout, mail); break;
    // -- the room -----------------------------------------------------------------
    case Act::kArrangeDesk: open_arrange_desk(); break;
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
        // AN OUTSIDE PRESS DISMISSES AND IS SPENT ON DISMISSING -- nothing beneath the press is
        // selected, focused or sent the press.
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
    switch (p.owner) {
    case PasteOwner::kNone: return;
    case PasteOwner::kNaming:
        box = naming_line();
        break;
    }
    if (box != nullptr && box->draft_epoch() == p.epoch) {
        if (a.readable) {
            session_.clipboard.text = a.text; // the platform's current truth, asked for
        }
        box->paste(session_.clipboard);
    } else {
        return; // the draft that asked is gone; the payload is discarded, silently
    }
    repaint(mail);
}

// WL-KEY-03 -- agents/workshop/keyboard.md
void WorkshopWeave::on(const zengine::input::TextEntered& t, loom::Mail& mail) {
    if (duplicate_input(mail)) return;
    if (quitting_) {
        HeldInput held;
        held.kind = HeldInput::Kind::kText;
        held.text = t;
        (void)hold_input(std::move(held));
        return;
    }
    // The swallow is judged before the gesture is counted: the character a consumed shortcut
    // produced is part of that shortcut's gesture (a terminal emits key, text and release for one
    // keystroke; SDL commits text on its own turn). Text that does not match is a new act.
    if (!swallow_text_.empty()) {
        const std::string owed = swallow_text_;
        swallow_text_.clear();
        if (same_keystroke(t.text, owed)) {
            return; // the character the trigger produced belongs to the trigger, not a new gesture
        }
    }
    ++gestures_;
    gesture_actor_ = input_actor_;
    if (t.text.empty()) {
        return;
    }
    // Where a character goes is where a key goes, by the same resolver: a mode owning the keyboard
    // takes the text or types none (arrangement is driven by letters), a focused pane receives it
    // as it receives keys, and in command mode text is not a command.
    switch (keyboard_context(session_)) {
    case KeyContext::kNaming:
        session_.setup.naming.line.type(t.text);
        repaint(mail);
        return;
    case KeyContext::kArrangePane:
    case KeyContext::kArrangeDesk:
    case KeyContext::kArrangeReset:
        return;
    case KeyContext::kPane:
        external_text(keyboard_pane(), t, mail);
        return;
    default:
        return; // command mode: text is simply not a command
    }
}

// WL-ARR-02 -- agents/workshop/arrangement.md; WL-TAB-11 -- agents/workshop/tab-run.md
WorkshopWeave::GesturesEnded WorkshopWeave::end_held_gestures() {
    GesturesEnded out;
    if (session_.pane_drag.active) {
        out.pane_held = true;
        out.pane = session_.pane_drag.pane;
        session_.pane_drag = PaneGesture{};
    }
    // The text-selection drag ends silently: the selection it swept is on screen and survives the
    // release, so only the gesture record is cleared.
    session_.text_drag = TextDrag{};
    // ...and so does the tab drag: the run's new order is on screen, narrated a step at a time.
    session_.tab_drag = LayoutTabDrag{};
    return out;
}

// WL-FOCUS-03 -- agents/workshop/focus.md; WL-PRESS-04 -- agents/workshop/press-chain.md
void WorkshopWeave::on(const zengine::input::PointerButton& b, loom::Mail& mail) {
    if (duplicate_input(mail)) return;
    if (quitting_) {
        HeldInput held;
        held.kind = HeldInput::Kind::kButton;
        held.button = b;
        (void)hold_input(std::move(held));
        return;
    }
    if (!b.pressed && release_value_drag(b, mail)) {
        (void)end_held_gestures(); repaint(mail); return;
    }
    if (!b.pressed && canvas_release(b, mail)) return;
    if (b.pressed && b.button >= 1 && b.button <= 3)
        lose_canvas_hold(static_cast<std::size_t>(b.button - 1), mail);
    // A press begins a gesture; its release completes that one and begins none, so the release of
    // a choosing click does not defeat its own continuation, and a newer key, character, press or
    // wheel still does (WL-PRESS-06).
    if (b.pressed) {
        ++gestures_;
        gesture_actor_ = input_actor_;
    }
    // Arrangement is a mode and owns the pointer while open: every press is about a pane. A
    // secondary press leaves the arrangement (any scope, the reset prompt included) and is consumed
    // whole, so no context menu opens from it: a state-local reading, not a Back command. A release
    // still ends a drag that began before the mode did, so no gesture is stranded.
    if (session_.arrange.open) {
        const PointedAt where = canvas_point_of(b.space, b.x, b.y);
        // A SECONDARY RELEASE ENDS A HOLD BEGUN BEFORE THIS MODE OPENED: a mode never occludes
        // a release (WL-PRESS-06).
        if (!b.pressed && (b.button == 2 || b.button == 3)) {
            (void)external_release(b.button, b, mail);
            return;
        }
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
    // A PANE'S MENU, PRESENTED BY ITS PRESENTER, HAS FIRST REFUSAL WHILE IT IS OPEN, on the
    // contextual surface's own terms: a press inside or outside it is spent on it and forwarded,
    // a secondary release ends its hold, and a right press withdraws it and is routed afresh below
    // exactly as it would have been with no menu open (`menu_button`).
    if (session_.presented.open) {
        if (menu_button(b, mail)) {
            repaint(mail);
            return;
        }
    }
    // The contextual surface has first refusal while open: a press inside navigates or chooses; a
    // press outside dismisses and is consumed, so a click spent closing a menu selects, focuses and
    // sends nothing. A further right press re-targets rather than toggles.
    if (session_.context.open) {
        const PointedAt where = canvas_point_of(b.space, b.x, b.y);
        // A SECONDARY RELEASE ENDS A HOLD BEGUN BEFORE THE SURFACE OPENED (WL-PRESS-06).
        if (!b.pressed && (b.button == 2 || b.button == 3)) {
            (void)external_release(b.button, b, mail);
            return;
        }
        if (b.pressed && b.button == 3) {
            if (where.understood) {
                // A FURTHER RIGHT PRESS RE-ASKS THE QUESTION about whatever is under it now: the
                // host's own menu, re-targeted. (A pane's menu -- the presenter's -- takes this
                // path in `menu_button`, which withdraws it and routes the press afresh.)
                open_context_at(where);
                repaint(mail);
            }
            return;
        }
        if (b.button != 1) {
            return;
        }
        if (!b.pressed) {
            // A release still ends a gesture that began before the surface opened: a right press
            // can arrive mid-drag.
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
    // THE SECOND BUTTON IS THE PANE'S FIRST (WL-PRESS-06). A secondary RELEASE is the hold's
    // pane's wherever the pointer is, and never asks the occupancy question; one that ends no
    // hold is dropped as every non-primary release always was.
    if (!b.pressed && (b.button == 2 || b.button == 3)) {
        if (external_release(b.button, b, mail)) {
            repaint(mail);
        }
        return;
    }
    // A secondary PRESS over a pane whose holder has the `PaneButton` door is DELIVERED, and
    // delivery is consumption: no menu, no selection change, no keyboard change. Only a press
    // that names a row of the BODY is the pane's; the chrome stays the host's, and a holder
    // without the door is sent nothing -- the host's own menu answers below, as it always did.
    if (b.pressed && (b.button == 2 || b.button == 3) && at.understood) {
        const Occupancy taker =
            occupied_at(session_.panels, session_.setup.active, screen_of(session_), at);
        if (taker.occupied && is_runtime_kind(taker.kind)) {
            if (canvas_press(taker.kind, b, typing_pane(session_) == taker.kind, mail)) {
                repaint(mail);
                return;
            }
            const ExternalPressAt aimed =
                external_press_at(session_.panels, session_.setup.active, screen_of(session_),
                                  taker.kind, session_.pane_titles, b.space, b.x, b.y);
            // A body press is the pane's, and empty by default: a holder without the door is sent
            // nothing and the press is still consumed. Silence is not pass-through; the host's own
            // menu is reached by the chrome and the Pane Manager, never by a right press in a
            // stranger's body (WL-CTX-08).
            if (aimed.named) {
                (void)external_button(taker.kind, b.button, aimed, at, mail);
                repaint(mail);
                return;
            }
        }
    }
    // A RIGHT PRESS NOBODY TOOK ASKS "WHAT CAN I DO WITH THIS?" -- the host's own surface, on
    // the chrome, the room, or a tab. A pane's BODY is not here: it was consumed above, with or
    // without a door. Only a press opens; a middle press nobody took is dropped below.
    if (b.pressed && b.button == 3 && at.understood) {
        // ...and a tab is a subject it can name, behind occupancy: the tab inverse is asked only
        // once the walk says the Layouts pane owns this point, so a covered tab is never named
        // through the pane covering it. A right press on `+` names the room: it is an action.
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
        // True means consumed: stop routing; false means carry on -- the only meaning the bools
        // below have. A layer that consumes may refuse, say nothing or change nothing, but a press
        // it owns is never answered by the layer around it. The occupancy walk is resolved first.
        const Occupancy here =
            occupied_at(session_.panels, session_.setup.active, screen_of(session_), at);
        // What the keyboard stood at is read before the lines below rewrite it: where an ordinary
        // key went, and where in a pane's body the press landed (a hidden-titles pane wears its
        // title row exactly while it has the keys). Which picture the press names is separate:
        // `external_press` stamps the one the medium held (`ExternalPane::stamp`).
        const std::int64_t typing_before = typing_pane(session_);
        const ExternalPressAt aimed =
            here.occupied && is_runtime_kind(here.kind)
                ? external_press_at(session_.panels, session_.setup.active, screen_of(session_),
                                    here.kind, session_.pane_titles, b.space, b.x, b.y)
                : ExternalPressAt{};
        if (drop_carry(here.kind, aimed, mail)) {
            repaint(mail);
            return;
        }
        const bool canvas_sent = here.occupied && is_runtime_kind(here.kind) &&
            canvas_press(here.kind, b, typing_before == here.kind, mail);
        // Which pane the maker pointed at, read once: selection is set for the whole rectangle
        // (header and padding included), and the keyboard candidate is derived from it through the
        // kind's declared candidacy (`PanelKind::takes_keyboard`). A press on the workspace, the
        // screen's furniture or nothing clears both; the modes above never reach this line.
        session_.panels.selected = here.occupied ? here.kind : kNoPaneKind;
        session_.panels.keyboard =
            session_.panels.selected != kNoPaneKind &&
                    kind_takes_keyboard(session_.panels.selected)
                ? session_.panels.selected
                : kNoPaneKind;
        // A press on a panel is that panel's; the bare room never hears it. A built-in says so on
        // the notice line rather than leaving the last gesture's sentence standing. A press on an
        // external pane is its provider's and consumed either way, decided here from geometry
        // Workshop holds; Workshop says nothing, since what it means is the pane's to say.
        if (here.occupied && is_runtime_kind(here.kind)) {
            // A press that named a body row takes hold of that pane for the length of the button:
            // the record is this host's, what the sweep means is the pane's (`external_drag`), and
            // the release ends it silently. A press on the header or padding begins no sweep.
            if (!canvas_sent && external_press(here.kind, aimed, typing_before == here.kind, mail)) {
                begin_value_drag(b);
                session_.text_drag.active = true;
                session_.text_drag.place = text_drag_place::kExternalPane;
                session_.text_drag.kind = here.kind;
            }
        } else if (here.occupied && here.kind == panel::kLayouts &&
                   layouts_press(b, mail)) {
            // ...and the Layouts pane's own inverse (tabs, `+`, the rename press, the reorder
            // drag), asked only once the walk says this point is that pane's.
            repaint(mail);
            return;
        } else if (here.occupied) {
            say(std::string(here.what) + " is here -- nothing under it can be taken hold of",
                false);
        } else {
            // The bare room: what stands there is the desktop's floor, which is words.
            say("nothing there", false);
        }
    } else {
        // A release is not asked the same question, which is why no capture state exists: a
        // gesture owns the pointer until it ends, so its release ends it wherever the hand is,
        // through the one owner (`end_held_gestures`); a press that began none leaves none.
        (void)end_held_gestures();
    }
    repaint(mail);
}

// WL-PANE-05 -- agents/workshop/panes-and-windows.md; WL-ARR-01 -- agents/workshop/arrangement.md
void WorkshopWeave::on(const zengine::input::PointerMoved& m, loom::Mail& mail) {
    if (duplicate_input(mail)) return;
    if (quitting_) {
        HeldInput held;
        held.kind = HeldInput::Kind::kMoved;
        held.moved = m;
        (void)hold_input(std::move(held));
        return;
    }
    if (move_value_drag(m, mail)) return;
    if (canvas_motion(m, mail)) return;

    // ---- Carrying a layout tab along the run -----------------------------------------
    // The hand holds the live layout (the press made it live), so a motion asks the press's
    // inverse against the run as painted now and moves the live layout to the tab it is over.
    // Order only: no desk is replaced and no provider hears it. Over no tab, `+` or its own span,
    // nothing moves, so dragging past the end rests rather than wraps.
    if (session_.tab_drag.active) {
        const LayoutTabPress over =
            band_tab_at(session_, screen_of(session_), m.space, m.x, m.y);
        if (over.hit && !over.create &&
            move_layout(session_.setup, session_.setup.active_at, over.at)) {
            repaint(mail);
        }
        return;
    }
    // Arrangement owns motion while open, for the press's reason; with no pane gesture held, a
    // motion does nothing.
    if (session_.arrange.open) {
        const PointedAt here = canvas_point_of(m.space, m.x, m.y);
        if (!here.understood || !session_.pane_drag.active) {
            return;
        }
        const PaneRef held = session_.pane_drag.pane;
        const SetupPane* before_row = pane_of(session_.setup.active, held);
        const std::optional<SetupPane> before =
            before_row != nullptr ? std::optional<SetupPane>(*before_row) : std::nullopt;
        const PaneRef addressed = session_.arrange.pane;
        const std::string notice = session_.notice;
        const bool bad = session_.notice_is_bad;
        // Interpret every motion, including refusals and loss of the held pane. Repeating
        // an accepted proposal can write the same values, so compare the resulting row
        // rather than treating a write attempt as a new picture.
        arrange_motion(here.sub.x, here.sub.y, mail);
        const SetupPane* after_row = pane_of(session_.setup.active, held);
        const bool changed_row = before.has_value()
                                     ? after_row == nullptr || *after_row != *before
                                     : after_row != nullptr;
        if (changed_row || !session_.pane_drag.active || !session_.arrange.open ||
            session_.arrange.pane != addressed || session_.notice != notice ||
            session_.notice_is_bad != bad ||
            conditions_taken_ != host_->conditions_generation) {
            repaint(mail);
        }
        return;
    }
    // A selection sweep in a pane this host did not compile: each motion is resolved against the
    // pane's body as it is now and crosses unclamped, since what a row past the edge means is the
    // pane's. Nothing here repaints; the pane's next content will.
    if (session_.text_drag.active &&
        session_.text_drag.place == text_drag_place::kExternalPane) {
        external_drag(session_.text_drag.kind, m, mail);
        return;
    }
}

// WL-PTR-10 -- agents/workshop/pointer.md
void WorkshopWeave::on(const zengine::input::PointerWheel& w, loom::Mail& mail) {
    if (duplicate_input(mail)) return;
    if (quitting_) {
        HeldInput held;
        held.kind = HeldInput::Kind::kWheel;
        held.wheel = w;
        (void)hold_input(std::move(held));
        return;
    }
    ++gestures_;
    gesture_actor_ = input_actor_;
    if (session_.arrange.open || session_.context.open || session_.presented.open) {
        return;
    }
    const Screen sc = screen_of(session_);
    const PointedAt at = canvas_point_of(w.space, w.x, w.y);
    if (!at.understood) {
        return;
    }
    // The topmost presentation under the wheel decides: scrolling something under another's pane
    // would be imaginary reach.
    const Occupancy here =
        occupied_at(session_.panels, session_.setup.active, sc, at);
    if (!here.occupied) {
        return;
    }
    if (is_runtime_kind(here.kind)) {
        if (!canvas_wheel(here.kind, w, mail)) external_wheel(here.kind, w, mail);
        return;
    }
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
    // A second press on the same tab renames it; the first already made it live, so the editor's
    // subject and the switch agree. The completing press spends the arming: no triple-click.
    const std::int64_t now = interaction_now();
    if (doubles_a_tab_click(session_.tab_click, tab.at, now)) {
        session_.tab_click = TabClickMemory{};
        open_layout_rename(tab.at);
        return true;
    }
    session_.tab_click = TabClickMemory{true, tab.at, now};
    // And the press takes hold of the tab: a press that never moves is the plain switch. The record
    // holds no position; what is carried is always `setup.active_at`.
    session_.tab_drag.active = true;
    switch_layout(tab.at, mail);
    return true;
}

} // namespace zengine::workshop
