// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s layouts: the live layout as its owner, `zengine.layouts`, sends it, presented
// here; every gesture on a layout asked of the owner; and the window's half of the last session.
// Workshop law: agents/workshop/layouts.md (+8 registers; agents/workshop.md routes)

#include "pane_migration.hpp"
#include "weave.hpp"
#include "window_persist.hpp"

#include <cstdio>

namespace zengine::workshop {

// ---- The live layout, presented --------------------------------------------------------------

// WL-LAYOUT-01 -- agents/workshop/layouts.md
void WorkshopWeave::on(const LiveLayout& live, loom::Mail& mail) {
    if (!mail.authored_from_role(kLayoutsRole)) {
        return; // the live layout is its owner's to say, and nobody else's picture is this desk
    }
    install_live(live, mail);
    // THE OWNER'S SENTENCE, said here: the restore's with the room it opened in, any other as it
    // is -- a written setting's says how far its hand-off got.
    if (live.restored) {
        say_restored(live, mail);
    } else if (!live.said.empty()) {
        say(live.said, live.bad);
    }
    // ONE GESTURE, ONE PICTURE: a picture its owner sends before answering this desk's edit is
    // painted with the answer, so what the gesture's continuation does is in the same repaint.
    if (edits_owed_.empty()) {
        repaint(mail);
    }
}

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
    // ANOTHER DESK LIVE -- another layout, or a desk put live in place of this one -- puts down the
    // menu open over the desk the weaver was on; an edit of the same desk leaves it standing.
    if (live.id != d.id || static_cast<std::uint64_t>(live.put_live) != d.put_live) {
        if (session_.presented.open) {
            withdraw_menu("another desk went live", mail);
        }
        if (session_.context.open) {
            close_context();
        }
    }
    const PaneRef palette{kPaletteRole, kPalettePane};
    const bool palette_seated = has_pane(d.active, palette);
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
    d.unused = live.unused;
    if (!live.session_refused.empty() && !session_refusal_said_) {
        // The notice is the event, said once; the condition is what stays true all run.
        session_refusal_said_ = true;
        say(live.session_refused + " -- opening with the default setup", true);
        session_.conditions.establish(
            Condition{kSessionWallKey, "session refused -- this run keeps no session",
                      live.session_refused +
                          " (the file is left exactly as it is, and this run will not write "
                          "over it)",
                      graphics::role::kAlert, std::string()});
    }
    settings_refused_ = live.settings_refused;
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
    // THE PANES FOLLOW THE DESK: whatever it seats is opened, whatever it no longer seats closes.
    // Each seated pane's settings are the owner's to hand it.
    apply_setup(mail);
    // THE PALETTE PUT DOWN, by whichever close took its row -- this desk's own or an office's,
    // which the owner took: the keys go back to the pane it opened over.
    if (palette_seated && !has_pane(d.active, palette)) {
        palette_closed(palette);
    }
}

// WL-LAYOUT-01 -- agents/workshop/layouts.md
void WorkshopWeave::ask_layout(std::vector<LayoutEdit> edits, EditThen then, loom::Mail& mail,
                               bool judged) {
    LayoutEditRequested asked;
    asked.revision = judged ? session_.setup.revision : 0;
    asked.edits = std::move(edits);
    const std::uint64_t ask = ++layout_asks_;
    if (mail.as_role(kWorkshopProvider).send_to_role(kLayoutsRole, std::move(asked), ask).valid()) {
        edits_owed_.emplace(ask, std::move(then));
        return;
    }
    // NOBODY KEEPS THE LAYOUTS: the desk stands as it is, and the gesture is refused in words.
    LayoutEditAnswered refused;
    refused.refusal = "nothing keeps this Workshop's layouts -- " + std::string(kLayoutsRole) +
                      " is not mounted, so the desk stands as it is";
    refused.revision = session_.setup.revision;
    if (then) {
        then(refused, mail);
    } else {
        say(refused.refusal, true);
    }
}

// WL-LAYOUT-01 -- agents/workshop/layouts.md
void WorkshopWeave::on(const LayoutEditAnswered& answered, loom::Mail& mail) {
    // AN ANSWER IS LOOM'S TO VERIFY, to an ask this weave sent; it is personal speech.
    if (!mail.answers_ask()) {
        return;
    }
    const auto owed = edits_owed_.find(mail.correlation());
    if (owed == edits_owed_.end()) {
        return; // an answer to an edit this desk no longer waits on
    }
    EditThen then = std::move(owed->second);
    edits_owed_.erase(owed);
    if (then) {
        then(answered, mail);
    } else if (!answered.accepted) {
        say(answered.refusal, true);
    }
    // THE DESK SETTLED: what waited for it is judged now, in order, until one asks again.
    while (edits_owed_.empty() && !settle_waiters_.empty()) {
        auto next = std::move(settle_waiters_.front());
        settle_waiters_.pop_front();
        next(mail);
    }
    repaint(mail);
}

void WorkshopWeave::seat_then(const PaneRef& ref, EditThen then, loom::Mail& mail, bool keys,
                              const std::optional<SetupPane>& window) {
    if (has_pane(session_.setup.active, ref)) {
        LayoutEditAnswered seated;
        seated.accepted = true;
        seated.revision = session_.setup.revision;
        then(seated, mail);
        return;
    }
    // THE WEAVER PLACES IT: a first open stands at the room's centre or the hand, never at a guess
    // at free room, and the arrangement that follows is the weaver's to end.
    PanePlace first{ui::kPlaceCenter, 0, 0};
    if (session_.open_at_mouse && session_.hand_px.has_value()) {
        const PixelRect at = room_of_canvas(
            PixelRect{session_.hand_px->x, session_.hand_px->y, 0, 0}, screen_of(session_));
        first = PanePlace{pane_unit::kPixels, (std::max)(std::int64_t{0}, at.x),
                          (std::max)(std::int64_t{0}, at.y)};
    }
    LayoutEdit seat;
    seat.op = layout_op::kSeat;
    seat.provider = ref.provider;
    seat.pane = ref.pane;
    seat.place = setup_persist::place_out(first);
    seat.width = setup_persist::size_out(PaneSize{});
    seat.height = setup_persist::size_out(PaneSize{});
    std::vector<LayoutEdit> edits{seat};
    const bool palette = ref.provider == kPaletteRole && ref.pane == kPalettePane;
    if (window.has_value()) {
        // ...A WINDOW ITS ASKER NAMES, where it was left or not: the Manual over its subject.
        SetupPane row = *window;
        row.ref = ref;
        edits.push_back(window_edit(row));
    } else if (palette) {
        // ...AND THE PALETTE AT THE CENTRE EVERY TIME, where it was left or not.
        LayoutEdit centred = seat;
        centred.op = layout_op::kWindow;
        centred.place = setup_persist::place_out(PanePlace{ui::kPlaceCenter, 0, 0});
        edits.push_back(std::move(centred));
    }
    const bool placed = palette || window.has_value();
    ask_layout(std::move(edits),
               [this, then = std::move(then), ref, keys, placed](const LayoutEditAnswered& seated,
                                                                 loom::Mail& m) {
                   then(seated, m);
                   // ...never over an arrangement the weaver has open: that is theirs to end.
                   if (seated.accepted && keys && !placed && !seated.returned &&
                       !session_.arrange.open && has_pane(session_.setup.active, ref)) {
                       enter_arrange_pane(ref, true);
                       repaint(m);
                   }
               },
               mail, false);
}

// WL-DESK-13 -- agents/workshop/desktop.md
void WorkshopWeave::unseat_then(const PaneRef& ref, EditThen then, loom::Mail& mail) {
    LayoutEdit unseat;
    unseat.op = layout_op::kUnseat;
    unseat.provider = ref.provider;
    unseat.pane = ref.pane;
    ask_layout({unseat}, std::move(then), mail, false);
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
    // SAID HERE ONLY WHILE THE PICTURE IS CURRENT: with an edit in flight, a layout this copy has
    // not heard of may stand, and the owner judges the step.
    if (edits_owed_.empty() && layout_count(session_.setup) <= 1) {
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
    // ONCE PER PROCESS: a window started again offers itself again, and an afternoon of arranging
    // must not be thrown back to a file written last night.
    if (restore_asked_) {
        return;
    }
    restore_asked_ = true;
    // THE WINDOW'S FACTS ARE WORKSHOP'S OWN FILE; while none exists, an older session's, once.
    const std::string window_path = window_persist::path_beside(host_->session_path);
    window_persist::LoadedWindow window = window_persist::load_file(window_path);
    if (!window.present) {
        window = window_persist::import_from_session(host_->session_path,
                                                     session_persist::kMaxSessionBytes);
    }
    if (window.present && !window.outcome.accepted) {
        // A WINDOW FILE THIS BUILD CANNOT READ opens at the default size, said; the desk comes back.
        const std::string why = window.outcome.refusal;
        window = window_persist::LoadedWindow{};
        window.present = true;
        window.declined = "the window file " + window_path + " could not be read -- " + why +
                          " -- opening at the default size";
    }
    if (window.present) {
        restore_window(window, mail);
    }
    // ...AND THE RUN, ASKED OF ITS OWNER, whose first answer is the last session restored; the
    // start's own notes are said after it, as the last word of the start.
    ask_layout({},
               [this](const LayoutEditAnswered& answered, loom::Mail& m) {
                   if (!answered.accepted) {
                       say(answered.refusal, true);
                   }
                   speak_startup_notes(m);
               },
               mail, false);
}

// WL-SESSION-14, WL-SESSION-16, WL-SESSION-17 -- agents/workshop/session-restore.md
void WorkshopWeave::restore_window(const window_persist::LoadedWindow& window, loom::Mail& mail) {
    const window_persist::Placement& placement = window.placement;
    // ---- The OS window's own room, kept whichever window this is ------------
    // A run in another window writes it back as it read it, as it does the placement.
    session_.window_room_w = placement.room_w;
    session_.window_room_h = placement.room_h;
    // ---- The room, then the desk into it ------------
    std::string declined = window.declined;
    std::int64_t room_w = window.viewport_w;
    std::int64_t room_h = window.viewport_h;
    bool honoured = window.honoured;
    if (window_name_ == graphics::kSdlWindow && placement.room_w > 0 && placement.room_h > 0) {
        room_w = placement.room_w;
        room_h = placement.room_h;
        honoured = window_persist::viewport_honoured(room_w, room_h);
        declined = honoured ? std::string() : window_persist::declined_viewport(room_w, room_h);
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
    restored_w_ = session_.screen_w;
    restored_h_ = session_.screen_h;
    window_declined_ = declined;
    // ---- The desktop placement, remembered and offered back --------------------
    if (placement.known) {
        session_.placement_known = true;
        session_.place_x = placement.x;
        session_.place_y = placement.y;
        session_.place_maximized = placement.maximized;
        const zengine::graphics::SurfacePlacementRemembered place{placement.x, placement.y,
                                                                  placement.maximized};
        if (restored_window_) {
            restored_window_->place = place; // after the shaping, which waits for the desk's area
        } else if (const std::string office = medium_office(); !office.empty()) {
            mail.send_to_role(office, place);
        }
    }
    // THE ROOM IS ASKED OF THE WINDOW NOW, before the window's own offer is taken: the desk comes
    // into it once its owner presents it.
    repaint(mail);
}

// WL-SESSION-14 -- agents/workshop/session-restore.md
void WorkshopWeave::say_restored(const LiveLayout& live, loom::Mail&) {
    RestoreNotice notice;
    notice.head = live.said;
    notice.w = restored_w_ > 0 ? restored_w_ : session_.screen_w;
    notice.h = restored_h_ > 0 ? restored_h_ : session_.screen_h;
    if (!window_declined_.empty()) {
        // AND IT NEVER CLAIMS THE SIZE CAME BACK WHEN IT DID NOT.
        notice.tail += "; " + window_declined_;
    }
    notice.tail += live.restore_tail;
    // ...and once, a choice it remembered that this plan no longer authors (WL-SWITCH-08).
    if (!host_->choice_note.empty()) {
        notice.tail += "; " + host_->choice_note;
    }
    notice.bad = !window_declined_.empty();
    // SAID ONCE, IN THE FIRST WINDOW'S UNIT: now if a window has offered a room, else when one does.
    restore_notice_ = std::move(notice);
    if (first_unit_) {
        (void)say_restore_notice(*first_unit_);
    }
}

// WL-SESSION-20 -- agents/workshop/window-file.md
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

// WL-SESSION-14 -- agents/workshop/session-restore.md
bool WorkshopWeave::say_restore_notice(std::int64_t cell_px) {
    if (!restore_notice_) {
        return false;
    }
    const RestoreNotice held = std::move(*restore_notice_);
    restore_notice_.reset();
    say(restore_notice_text(held, cell_px), held.bad);
    return true;
}

// WL-SESSION-13, WL-SESSION-15 -- agents/workshop/session.md; WL-SESSION-16 -- agents/workshop/session-restore.md
// WL-MIG-10 -- agents/workshop/migration.md
bool WorkshopWeave::save_last_session(loom::Mail& mail) {
    // The viewport written is the normal window's, so a maximized close remembers the room the
    // weaver chose, with the maximized state beside it. The placement rides along as last reported,
    // or as the file carried it; and the OS window's own room is this run's where the desk is in
    // the OS window, else the one the file carried.
    window_persist::Placement place;
    place.known = session_.placement_known;
    place.x = session_.place_x;
    place.y = session_.place_y;
    place.maximized = session_.place_maximized;
    const bool os_window = window_name_ == graphics::kSdlWindow;
    place.room_w = os_window ? session_.normal_w : session_.window_room_w;
    place.room_h = os_window ? session_.normal_h : session_.window_room_h;
    // THE WINDOW FILE FIRST, Workshop's own; then the owner is asked to save the run. Neither is
    // written where the session could not be read: the window's facts belong to that session.
    const std::string window_path = window_persist::path_beside(host_->session_path);
    if (!window_path.empty() && !session_refusal_said_) {
        if (const Written w = window_persist::save_file(window_path, session_.normal_w,
                                                        session_.normal_h, place);
            !w.accepted) {
            say("the window file was not written -- " + w.refusal, true);
            std::fprintf(stderr, "zengine-workshop: the window file was not written -- %s\n",
                         w.refusal.c_str());
        }
    }
    return mail.as_role(kWorkshopProvider).send_to_role(kLayoutsRole, SessionSaveRequested{}).valid();
}

// WL-SESSION-13 -- agents/workshop/session.md
void WorkshopWeave::on(const SessionSaveAnswered& answered, loom::Mail& mail) {
    if (!mail.answers_ask() || !saving_) {
        return;
    }
    saving_ = false;
    if (!answered.written && !host_->session_path.empty() && !session_refusal_said_) {
        // Where a failure goes when the screen is what is leaving: the notice, because nothing
        // fails silently and a suite reads it, and stderr, because the weaver will never see it.
        say(answered.refusal, true);
        std::fprintf(stderr, "zengine-workshop: %s\n", answered.refusal.c_str());
        std::fflush(stderr);
    }
    end_quit(mail);
}

} // namespace zengine::workshop} // namespace zengine::workshop
