// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s layouts: the live layout as the store keeps it, presented here; every gesture
// on a layout asked of the store; and the window's half of the last session.
// Workshop law: agents/workshop/layouts.md (+8 registers; agents/workshop.md routes)

#include "pane_migration.hpp"
#include "weave.hpp"

namespace zengine::workshop {

void WorkshopWeave::on(const SetupApplyRequested& request, loom::Mail& mail) {
    if (session_.presented.open) {
        (void)mail.answer(loom::Refused{"close the current menu before applying a setup"});
        return;
    }
    const std::string refused = store().apply_setup(request.setup);
    if (!refused.empty()) {
        (void)mail.answer(loom::Refused{refused});
        return;
    }
    session_.panes.selected = kNoPaneKind;
    session_.panes.keyboard = kNoPaneKind;
    take_store(mail);
    repaint(mail);
    (void)mail.answer(loom::Ack{});
}

// ---- The live layout, presented --------------------------------------------------------------

// WL-LAYOUT-01 -- agents/workshop/layouts.md
void WorkshopWeave::install_live(const LiveLayout& live, loom::Mail& mail) {
    Setup desk;
    if (const Written understood = setup_persist::setup_in(live.desk, desk); !understood.accepted) {
        // A COPY THIS DESK CANNOT READ IS NO PICTURE: the one it holds stands, and the defect is
        // said.
        say("defect: the live layout could not be presented -- " + understood.refusal, true);
        return;
    }
    LiveDesk& d = session_.setup;
    d.owned = desk;
    d.active = std::move(desk);
    d.id = live.id;
    d.active_at = static_cast<std::size_t>(live.position < 0 ? 0 : live.position);
    d.tabs = live.tabs;
    if (d.tabs.empty()) {
        d.tabs.push_back(LiveTab{live.id, d.active.name, setup_link::kNone});
    }
    d.link_path = live.link_path;
    d.link = d.active_at < d.tabs.size() ? d.tabs[d.active_at].link : setup_link::kNone;
    d.put_live = static_cast<std::uint64_t>(live.put_live);
    d.revision = live.revision;
    d.revisions = live.revisions;
    if (!live.session_refused.empty()) {
        // The notice is the event; the condition is what stays true all run.
        session_.conditions.establish(
            Condition{kSessionWallKey, "session refused -- this run keeps no session",
                      live.session_refused +
                          " (the file is left exactly as it is, and this run will not write "
                          "over it)",
                      graphics::role::kAlert, std::string()});
    }
    if (!live.settings_refused.empty()) {
        session_.conditions.establish(Condition{kPrefsWallKey,
                                                "preferences refused -- defaults stand",
                                                live.settings_refused, graphics::role::kAlert,
                                                std::string()});
    }
    // A SEAT STILL OWED TO THE STORE STAYS PAINTED over the desk it sent.
    if (seat_owed_) {
        (void)add_pane(d.active, *seat_owed_);
    }
    // THE PANES FOLLOW THE DESK: whatever it seats is opened, whatever it no longer seats closes,
    // and each seated pane is handed its row's settings before any room.
    apply_setup(mail);
    hand_settings(mail);
}

// WL-LAYOUT-01 -- agents/workshop/layouts.md
void WorkshopWeave::ask_layout(std::vector<LayoutEdit> edits, EditThen then, loom::Mail& mail,
                               bool judged) {
    LayoutEditRequested asked;
    asked.revision = judged ? session_.setup.revision : 0;
    asked.edits = std::move(edits);
    const LayoutEditAnswered answered = store().edit(asked);
    take_store(mail);
    if (then) {
        then(answered, mail);
    } else if (!answered.accepted) {
        say(answered.refusal, true);
    }
}

void WorkshopWeave::seat_then(const PaneRef& ref, EditThen then, loom::Mail& mail) {
    if (has_pane(session_.setup.active, ref)) {
        LayoutEditAnswered seated;
        seated.accepted = true;
        seated.revision = session_.setup.revision;
        then(seated, mail);
        return;
    }
    LayoutEdit seat;
    seat.op = layout_op::kSeat;
    seat.provider = ref.provider;
    seat.pane = ref.pane;
    ask_layout({seat}, std::move(then), mail, false);
}

// ---- The setup: name it, save it, restore it ------------------------------

// WL-LAYOUT-05, WL-LAYOUT-07 -- agents/workshop/layouts.md
// WL-ARR-03 -- agents/workshop/arrangement.md
// WL-PED-05 -- agents/workshop/pane-manager.md
void WorkshopWeave::apply_setup(loom::Mail& mail) {
    apply_setup_now();
    // A HOLD, A CONTINUATION OR A PRESENTED MENU ON A PANE THAT JUST LEFT THE DESK ends aloud:
    // the hold with a `lost` release, the continuation silently (closing invalidates it whether
    // or not the button is still down), the menu answered unchosen. (WL-PRESS-06)
    end_lost_holds(mail);
}

void WorkshopWeave::apply_setup_now() {
    // Membership-dependent session state first: every membership change comes through this door,
    // so it is the one place that notices a selection whose pane is no longer named.
    forget_removed_selection();
    (void)reconcile(session_.panes, session_.setup.active);
}

// WL-CTX-07 -- agents/workshop/contextual.md
// WL-LAYOUT-10 -- agents/workshop/layouts.md
void WorkshopWeave::open_layout_rename(std::size_t at) {
    if (at >= layout_count(session_.setup)) {
        return; // the belt: a captured position the run no longer holds
    }
    LayoutNaming& naming = session_.setup.naming;
    naming.open = true;
    naming.at = at;
    const std::string& name = layout_name_at(session_.setup, at);
    naming.line.set(name, name.size());
    say("rename this layout -- " + hotkey(Act::kNamingCommit) + " renames it, " +
            hotkey(Act::kNamingCancel) + " cancels",
        false);
}

void WorkshopWeave::naming_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
    LayoutNaming& naming = session_.setup.naming;
    // The line's own vocabulary first; what a committed name means, and what abandoning one leaves
    // standing, stay here.
    if (naming.line.consume(k.scancode, k.modifiers, session_.clipboard)) {
        return;
    }
    switch (session_.keymap.action_for(KeyContext::kNaming, k.scancode, k.modifiers)) {
    case Act::kNamingCommit: commit_layout_rename(mail); break;
    case Act::kNamingCancel:
        close_naming();
        say("the layout name is unchanged", false);
        break;
    default: break;
    }
}

void WorkshopWeave::close_naming() {
    session_.setup.naming = LayoutNaming{};
}

// WL-LAYOUT-04, WL-LAYOUT-10 -- agents/workshop/layouts.md
void WorkshopWeave::commit_layout_rename(loom::Mail& mail) {
    LayoutNaming& naming = session_.setup.naming;
    const std::size_t at = naming.at;
    const std::string wanted = naming.line.text();
    const Written legal = check_setup_name(wanted);
    if (!legal.accepted) {
        say(legal.refusal + " -- " + hotkey(Act::kNamingCommit) + " tries again, " +
                hotkey(Act::kNamingCancel) + " cancels",
            true);
        return;
    }
    if (at >= layout_count(session_.setup)) {
        close_naming();
        say("that layout is no longer here -- nothing was renamed", true);
        return;
    }
    LayoutEdit rename;
    rename.op = layout_op::kRename;
    rename.layout = layout_id_at(session_.setup, at);
    rename.name = wanted;
    // A REFUSED NAME KEEPS THE LINE OPEN, the owner's words said; an accepted one closes it.
    ask_layout({rename},
               [this](const LayoutEditAnswered& answered, loom::Mail&) {
                   if (!answered.accepted) {
                       say(answered.refusal, true);
                       return;
                   }
                   close_naming();
               },
               mail, false);
}

// WL-LAYOUT-10 -- agents/workshop/layouts.md
// WL-GUEST-09 -- agents/workshop/guests.md
void WorkshopWeave::save_setup(loom::Mail& mail) {
    // A FILE WRITE, judged for the hand that asked (`actor_scope.hpp`): a guest's on a weaver's host
    // writes nothing.
    if (const std::string refused = judge_classes(input_actor_, {scope::kWrite}, {}); !refused.empty()) {
        say("the setup was not saved -- " + refused, true);
        return;
    }
    LayoutEdit save;
    save.op = layout_op::kSave;
    ask_layout({save}, nullptr, mail, false);
}

// WL-LAYOUT-09, WL-LAYOUT-10, WL-LAYOUT-11 -- agents/workshop/layouts.md
void WorkshopWeave::restore_setup(loom::Mail& mail) {
    LayoutEdit restore;
    restore.op = layout_op::kRestore;
    ask_layout({restore}, nullptr, mail, false);
}

// ---- THE LAYOUT SHELF: several desks, one of them live --------------------

void WorkshopWeave::switch_layout(std::size_t to, loom::Mail& mail) {
    LayoutEdit sw;
    sw.op = layout_op::kSwitch;
    sw.layout = layout_id_at(session_.setup, to);
    ask_layout({sw}, nullptr, mail, false);
}

void WorkshopWeave::step_layout(std::int64_t by, loom::Mail& mail) {
    if (layout_count(session_.setup) <= 1) {
        say("this is the only layout -- " + hotkey(Act::kLayoutNew) + " makes another",
            false);
        return;
    }
    LayoutEdit step;
    step.op = layout_op::kStep;
    step.by = by;
    ask_layout({step}, nullptr, mail, false);
}

std::string WorkshopWeave::layout_ceiling_note() const {
    return "that is the most layouts one Workshop keeps (" +
           std::to_string(kMaxLayouts) + ") -- " + hotkey(Act::kLayoutRemove) +
           " drops this one";
}

// WL-LAYOUT-03 -- agents/workshop/layouts.md
void WorkshopWeave::new_layout(loom::Mail& mail) {
    if (layout_count(session_.setup) >= kMaxLayouts) {
        say(layout_ceiling_note(), true);
        return;
    }
    LayoutEdit made;
    made.op = layout_op::kNew;
    ask_layout({made}, nullptr, mail, false);
}

// WL-CTX-07 -- agents/workshop/contextual.md; WL-LAYOUT-04 -- agents/workshop/layouts.md
void WorkshopWeave::duplicate_layout(std::size_t at, loom::Mail& mail) {
    if (at >= layout_count(session_.setup)) {
        return; // the belt: a captured position the run no longer holds
    }
    if (layout_count(session_.setup) >= kMaxLayouts) {
        say(layout_ceiling_note(), true);
        return;
    }
    LayoutEdit copy;
    copy.op = layout_op::kDuplicate;
    copy.layout = layout_id_at(session_.setup, at);
    ask_layout({copy}, nullptr, mail, false);
}

// WL-CTX-07 -- agents/workshop/contextual.md; WL-LAYOUT-03 -- agents/workshop/layouts.md
void WorkshopWeave::drop_layout(std::size_t at, loom::Mail& mail) {
    if (at >= layout_count(session_.setup)) {
        return; // the belt: a captured position the run no longer holds
    }
    LayoutEdit drop;
    drop.op = layout_op::kRemove;
    drop.layout = layout_id_at(session_.setup, at);
    ask_layout({drop}, nullptr, mail, false);
}

// WL-CTX-07 -- agents/workshop/contextual.md; WL-LAYOUT-04 -- agents/workshop/layouts.md
void WorkshopWeave::shift_layout(std::size_t at, std::int64_t by, loom::Mail& mail) {
    const std::size_t n = layout_count(session_.setup);
    if (at >= n) {
        return; // the belt: a captured position the run no longer holds
    }
    if ((by < 0 && at == 0) || (by > 0 && at + 1 == n)) {
        say("layout " + quoted_setup_name(layout_name_at(session_.setup, at)) +
                " is already at the " + (by < 0 ? "start" : "end") + " of the run",
            false);
        return;
    }
    LayoutEdit move;
    move.op = layout_op::kMove;
    move.layout = layout_id_at(session_.setup, at);
    move.by = static_cast<std::int64_t>(by < 0 ? at - 1 : at + 1);
    ask_layout({move}, nullptr, mail);
}

// WL-PANE-10 -- agents/workshop/panes-and-windows.md
std::string WorkshopWeave::unresolved_note(const Setup& s) const {
    const std::vector<PaneRef> waiting = unresolved_panes(s, session_.panes);
    if (waiting.empty()) {
        return {};
    }
    std::string note = " -- " + std::to_string(waiting.size()) +
                       (waiting.size() == 1 ? " pane" : " panes") + " unresolved: " +
                       ref_text(waiting.front());
    if (waiting.size() > 1) {
        note += ", ...";
    }
    return note;
}

// ---- THE LAST SESSION: the desk that comes back on its own ----------------

// WL-SESSION-14, WL-SESSION-16, WL-SESSION-17 -- agents/workshop/session-restore.md
// WL-MIG-10 -- agents/workshop/migration.md
void WorkshopWeave::restore_last_session(loom::Mail& mail) {
    // ONCE PER PROCESS, and the store keeps the guard: a window's offer is not a once-per-process
    // fact, and a window started again offers itself again.
    const RestoredRoom last = store().restore();
    if (!store_.session_refusal().empty() && !last.present) {
        say(store_.session_refusal() + " -- opening with the default setup", true);
        take_store(mail);
        return;
    }
    if (!last.present) {
        take_store(mail);
        return;
    }
    // ---- The OS window's own room, kept whichever window this is ------------
    // A run in another window writes it back as it read it, as it does the placement.
    session_.window_room_w = last.placement.room_w;
    session_.window_room_h = last.placement.room_h;
    // ---- The room, then the desk into it ------------
    std::string declined = last.declined;
    std::int64_t room_w = last.viewport_w;
    std::int64_t room_h = last.viewport_h;
    bool honoured = last.honoured;
    if (window_name_ == graphics::kSdlWindow && last.placement.room_w > 0 &&
        last.placement.room_h > 0) {
        room_w = last.placement.room_w;
        room_h = last.placement.room_h;
        honoured = session_persist::viewport_honoured(room_w, room_h);
        declined = honoured ? std::string() : session_persist::declined_viewport(room_w, room_h);
        if (honoured) {
            restored_window_ = RestoredWindow{room_w, room_h, std::nullopt};
        }
    }
    if (honoured && adopt_screen(session_, room_w, room_h, session_.text_advance_px,
                                 session_.text_line_px, session_.cell_px)) {
        // The restored room IS the normal window's room -- the save wrote it from exactly that --
        // so the remembered pair starts equal to it rather than waiting for the first extent.
        session_.normal_w = session_.screen_w;
        session_.normal_h = session_.screen_h;
    }
    // ---- The desktop placement, remembered and offered back --------------------
    if (last.placement.known) {
        session_.placement_known = true;
        session_.place_x = last.placement.x;
        session_.place_y = last.placement.y;
        session_.place_maximized = last.placement.maximized;
        const zengine::graphics::SurfacePlacementRemembered place{
            last.placement.x, last.placement.y, last.placement.maximized};
        if (restored_window_) {
            restored_window_->place = place; // after the shaping, which waits for the desk's area
        } else if (const std::string office = medium_office(); !office.empty()) {
            mail.send_to_role(office, place);
        }
    }
    // ---- ...and then the desks, into the room they asked for ---------------
    take_store(mail);
    RestoreNotice notice;
    notice.head = last.head;
    notice.w = session_.screen_w;
    notice.h = session_.screen_h;
    if (!declined.empty()) {
        // AND IT NEVER CLAIMS THE SIZE CAME BACK WHEN IT DID NOT.
        notice.tail += "; " + declined;
    }
    if (!last.converted.empty()) {
        notice.tail += "; " + last.converted;
    }
    // ...and once, a choice it remembered that this plan no longer authors (WL-SWITCH-08).
    if (!host_->choice_note.empty()) {
        notice.tail += "; " + host_->choice_note;
    }
    notice.bad = !declined.empty();
    say(restore_notice_text(notice, session_.cell_px), notice.bad);
    notice.said = notices_said_;
    restore_notice_ = std::move(notice);
    // THE SECOND PICTURE OF THE RUN, and the one that asks for the room -- see `start_desk` for
    // why it cannot be the first.
    repaint(mail);
}

// WL-SESSION-20 -- agents/workshop/session-restore.md
void WorkshopWeave::shape_restored_window(loom::Mail& mail) {
    if (!restored_window_ || window_name_ != graphics::kSdlWindow) {
        return;
    }
    const RestoredWindow held = std::move(*restored_window_);
    restored_window_.reset();
    const std::string office = medium_office();
    // An empty title keeps the window's own: the room is the restore's, the title is not.
    mail.as_role(kWorkshopProvider)
        .send_to_role(office, zengine::graphics::WindowShaped{graphics::kSdlWindow, std::string(),
                                                              held.w, held.h});
    if (held.place) {
        mail.send_to_role(office, *held.place);
    }
}

// WL-SESSION-14 -- agents/workshop/session-restore.md
std::string WorkshopWeave::restore_notice_text(const RestoreNotice& n, std::int64_t cell_px) {
    bool projected = false;
    std::string said = n.head + " -- " + geometry_amount_text(n.w, cell_px, projected) + "x" +
                       geometry_amount_text(n.h, cell_px, projected) + " " + geometry_unit(cell_px);
    if (projected) {
        said += kProjectedNote;
    }
    return said + n.tail;
}

bool WorkshopWeave::spell_restore_notice(std::int64_t cell_px) {
    if (!restore_notice_) {
        return false;
    }
    const RestoreNotice held = std::move(*restore_notice_);
    restore_notice_.reset();
    if (held.said != notices_said_) {
        return false; // something was said since, and it stands
    }
    say(restore_notice_text(held, cell_px), held.bad);
    return true;
}

// WL-SESSION-13, WL-SESSION-15 -- agents/workshop/session.md; WL-SESSION-16 -- agents/workshop/session-restore.md
// WL-MIG-10 -- agents/workshop/migration.md
void WorkshopWeave::save_last_session(loom::Mail&) {
    // The viewport written is the normal window's, so a maximized close remembers the room the
    // weaver chose, with the maximized state beside it. The placement rides along as last reported,
    // or as the file carried it; and the OS window's own room is this run's where the desk is in
    // the OS window, else the one the file carried.
    session_persist::Placement place;
    place.known = session_.placement_known;
    place.x = session_.place_x;
    place.y = session_.place_y;
    place.maximized = session_.place_maximized;
    const bool os_window = window_name_ == graphics::kSdlWindow;
    place.room_w = os_window ? session_.normal_w : session_.window_room_w;
    place.room_h = os_window ? session_.normal_h : session_.window_room_h;
    const std::string refused = store().save_session(session_.normal_w, session_.normal_h, place);
    if (refused.empty() || host_->session_path.empty() || !store_.session_refusal().empty()) {
        return;
    }
    // Where a failure goes when the screen is what is leaving: the notice, because nothing fails
    // silently and a suite reads it, and stderr, because the weaver will never see that notice.
    say(refused, true);
    std::fprintf(stderr, "zengine-workshop: %s\n", refused.c_str());
    std::fflush(stderr);
}

} // namespace zengine::workshop
