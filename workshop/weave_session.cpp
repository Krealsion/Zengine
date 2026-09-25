// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s setup, layout shelf and last session.
// Workshop law: agents/workshop/layouts.md (+8 registers; agents/workshop.md routes)

#include "pane_migration.hpp"
#include "weave.hpp"

namespace zengine::workshop {

void WorkshopWeave::on(const SetupApplyRequested& request, loom::Mail& mail) {
    if (session_.presented.open) {
        (void)mail.answer(loom::Refused{"close the current menu before applying a setup"}); return;
    }
    const auto loaded = setup_persist::from_text(request.setup);
    if (!loaded.outcome.accepted) {
        (void)mail.answer(loom::Refused{loaded.outcome.refusal}); return;
    }
    const auto seating = seat_panes(loaded.setup, session_.panels, stack_capacity(screen_of(session_)));
    if (seating.unresolved || !seating.waiting.empty()) {
        (void)mail.answer(loom::Refused{"setup has unresolved panes or panes waiting for room"}); return;
    }
    session_.setup.active = loaded.setup;
    session_.setup.active_link = {};
    ++session_.setup.put_live;
    session_.panels.selected = kNoPaneKind;
    session_.panels.keyboard = kNoPaneKind;
    apply_setup(mail);
    say("applied setup " + quoted_setup_name(loaded.setup.name), false);
    repaint(mail);
    (void)mail.answer(loom::Ack{});
}

// ---- The setup: name it, save it, restore it ------------------------------

// WL-LAYOUT-05, WL-LAYOUT-07 -- agents/workshop/layouts.md
// WL-ARR-03 -- agents/workshop/arrangement.md
// WL-MAKER-09 -- agents/workshop/maker-pane.md
// WL-PED-05 -- agents/workshop/pane-manager.md
// WL-SESSION-12 -- agents/workshop/session-restore.md
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
    const Reconciled done = reconcile(session_.panels, session_.setup.active,
                                      stack_capacity(screen_of(session_)));
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
    const std::string& name = layout_at(session_.setup, at).name;
    naming.line.set(name, name.size());
    say("rename this layout -- " + hotkey(Act::kNamingCommit) + " renames it, " +
            hotkey(Act::kNamingCancel) + " cancels",
        false);
}

void WorkshopWeave::naming_key(const zengine::input::KeyPressed& k, loom::Mail&) {
    LayoutNaming& naming = session_.setup.naming;
    // The line's own vocabulary first; what a committed name means, and what abandoning one leaves
    // standing, stay here.
    if (naming.line.consume(k.scancode, k.modifiers, session_.clipboard)) {
        return;
    }
    switch (session_.keymap.action_for(KeyContext::kNaming, k.scancode, k.modifiers)) {
    case Act::kNamingCommit: commit_layout_rename(); break;
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
void WorkshopWeave::commit_layout_rename() {
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
    Setup candidate = layout_at(session_.setup, at);
    candidate.name = wanted;
    const Written whole = check_setup(candidate);
    if (!whole.accepted) {
        say(whole.refusal, true);
        return;
    }
    rename_layout(session_.setup, at, wanted);
    close_naming();
    // No `apply_setup`: a name is the one authored field no presentation reads. The painted row
    // and the link status are derived at the next composition.
    say("renamed layout " + quoted_setup_name(wanted) + link_note(at), false);
}

// WL-LAYOUT-02 -- agents/workshop/layouts.md
std::string WorkshopWeave::link_note(std::size_t at) const {
    const SetupLink& link = link_at(session_.setup, at);
    const std::int64_t status = link_status(layout_at(session_.setup, at), link);
    if (status == setup_link::kNone) {
        return {};
    }
    return std::string(" -- ") + link.path + " is " +
           (status == setup_link::kCurrent ? kSetupLinkCurrent : kSetupLinkModified);
}

// WL-LAYOUT-10 -- agents/workshop/layouts.md
const std::string& WorkshopWeave::setup_artifact() const {
    return session_.setup.active_link.path.empty() ? host_->setup_path
                                                   : session_.setup.active_link.path;
}

// WL-LAYOUT-09, WL-LAYOUT-10, WL-LAYOUT-11 -- agents/workshop/layouts.md
void WorkshopWeave::save_setup() {
    const std::string path = setup_artifact();
    if (path.empty()) {
        say(kNoSetupFile, true);
        return;
    }
    const Setup& desk = session_.setup.active;
    const Written whole = check_setup(desk);
    if (!whole.accepted) {
        say(whole.refusal, true);
        return;
    }
    const Written written = setup_persist::save_file(path, desk);
    if (!written.accepted) {
        // THE LAST GOOD SETUP FILE IS INTACT and so is every association: the
        // writer never opened the destination, and nothing below this line has
        // run.
        say(written.refusal, true);
        return;
    }
    session_.setup.active_link.path = path;
    adopt_known_setup(session_.setup, path, desk);
    say("saved setup " + quoted_setup_name(desk.name) + " to " + path +
            unresolved_note(desk),
        false);
}

// WL-LAYOUT-09, WL-LAYOUT-10, WL-LAYOUT-11 -- agents/workshop/layouts.md
void WorkshopWeave::restore_setup(loom::Mail& mail) {
    const std::string path = setup_artifact();
    if (path.empty()) {
        say(kNoSetupFile, true);
        return;
    }
    const setup_persist::LoadedSetup loaded = setup_persist::load_file(path);
    if (!loaded.outcome.accepted) {
        say(loaded.outcome.refusal, true);
        return;
    }
    session_.setup.active = loaded.setup;
    ++session_.setup.put_live; // the file's desk, not the one it replaced (WL-INFO-14)
    session_.setup.active_link.path = path;
    adopt_known_setup(session_.setup, path, loaded.setup);
    apply_setup(mail);
    // AND IF THIS FILE NAMED A PANE THAT CHANGED HANDS, IT SAYS SO -- here as well as on
    // the session road, because a maker who explicitly restored a setup they wrote months
    // ago is the maker most likely to go and look at the file afterwards.
    say("restored setup " + quoted_setup_name(loaded.setup.name) + " from " + path +
            unresolved_note(loaded.setup) +
            (loaded.converted.total() > 0
                 ? "; " + pane_migration::converted_note(loaded.converted)
                 : std::string()),
        false);
}

// ---- THE LAYOUT SHELF: several desks, one of them live --------------------

std::string WorkshopWeave::layout_note() const {
    return "layout " + quoted_setup_name(session_.setup.active.name) + " -- " +
           std::to_string(session_.setup.active_at + 1) + " of " +
           std::to_string(layout_count(session_.setup));
}

void WorkshopWeave::switch_layout(std::size_t to, loom::Mail& mail) {
    if (!activate_layout(session_.setup, to)) {
        // NOTHING MOVED: the position is the live layout's own, or is not a layout.
        // Said rather than silent, because a maker who pressed the tab they are
        // already on has aimed at something and is owed the row's own answer.
        say(layout_note() + link_note(session_.setup.active_at), false);
        return;
    }
    apply_setup(mail);
    say(layout_note() + link_note(session_.setup.active_at) +
            unresolved_note(session_.setup.active),
        false);
}

void WorkshopWeave::step_layout(std::int64_t by, loom::Mail& mail) {
    if (layout_count(session_.setup) <= 1) {
        say("this is the only layout -- " + hotkey(Act::kLayoutNew) + " makes another",
            false);
        return;
    }
    switch_layout(layout_step(session_.setup, by), mail);
}

std::string WorkshopWeave::layout_ceiling_note() const {
    return "that is the most layouts one Workshop keeps (" +
           std::to_string(kMaxLayouts) + ") -- " + hotkey(Act::kLayoutRemove) +
           " drops this one";
}

// WL-LAYOUT-03 -- agents/workshop/layouts.md
void WorkshopWeave::new_layout(loom::Mail& mail) {
    if (!add_layout(session_.setup)) {
        say(layout_ceiling_note(), true);
        return;
    }
    apply_setup(mail);
    say("new " + layout_note() + " -- an empty desk", false);
}

// WL-CTX-07 -- agents/workshop/contextual.md; WL-LAYOUT-04 -- agents/workshop/layouts.md
void WorkshopWeave::duplicate_layout(std::size_t at, loom::Mail& mail) {
    if (at >= layout_count(session_.setup)) {
        return; // the belt: a captured position the run no longer holds
    }
    if (!::zengine::workshop::duplicate_layout(session_.setup, at)) {
        say(layout_ceiling_note(), true);
        return;
    }
    apply_setup(mail);
    say("duplicated " + layout_note() + " -- the desk was copied, its setup file was not",
        false);
}

// WL-CTX-07 -- agents/workshop/contextual.md; WL-LAYOUT-03 -- agents/workshop/layouts.md
void WorkshopWeave::drop_layout(std::size_t at, loom::Mail& mail) {
    if (at >= layout_count(session_.setup)) {
        return; // the belt: a captured position the run no longer holds
    }
    const bool was_live = at == session_.setup.active_at;
    const std::string gone = quoted_setup_name(layout_at(session_.setup, at).name);
    if (!remove_layout(session_.setup, at)) {
        say("this is the only layout -- Workshop always has one desk", true);
        return;
    }
    if (was_live) {
        apply_setup(mail);
    }
    say("removed layout " + gone + " -- now on " + layout_note(), false);
}

// WL-CTX-07 -- agents/workshop/contextual.md; WL-LAYOUT-04 -- agents/workshop/layouts.md
void WorkshopWeave::shift_layout(std::size_t at, std::int64_t by) {
    const std::size_t n = layout_count(session_.setup);
    if (at >= n) {
        return; // the belt: a captured position the run no longer holds
    }
    if ((by < 0 && at == 0) || (by > 0 && at + 1 == n)) {
        say("layout " + quoted_setup_name(layout_at(session_.setup, at).name) +
                " is already at the " + (by < 0 ? "start" : "end") + " of the run",
            false);
        return;
    }
    const std::size_t to = by < 0 ? at - 1 : at + 1;
    const std::string moved = quoted_setup_name(layout_at(session_.setup, at).name);
    if (!move_layout(session_.setup, at, to)) {
        return;
    }
    say("moved layout " + moved + " to " + std::to_string(to + 1) + " of " +
            std::to_string(n),
        false);
}

// WL-PANE-10 -- agents/workshop/panes-and-windows.md
std::string WorkshopWeave::unresolved_note(const Setup& s) const {
    const std::vector<PaneRef> waiting = unresolved_panes(s, session_.panels);
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

// WL-SESSION-11, WL-SESSION-12, WL-SESSION-14, WL-SESSION-16, WL-SESSION-17 -- agents/workshop/session-restore.md
// WL-MAKER-09 -- agents/workshop/maker-pane.md
// WL-MIG-10 -- agents/workshop/migration.md
void WorkshopWeave::restore_last_session(loom::Mail& mail) {
    if (restored_) {
        return;
    }
    // ONCE PER PROCESS, and the guard is HERE rather than at the caller because
    // `SurfaceReady` is not a once-per-process fact: a Skin replacement announces itself
    // again, and a maker whose afternoon of arranging was silently thrown back to a file
    // written last night would have met a continuity feature that loses work.
    restored_ = true;
    if (host_->session_path.empty()) {
        return; // no session file was chosen: restore nothing, and say nothing about it
    }
    // ...and whatever conversions this run has: a reading taken now, not a capability this weave
    // holds. An older session is brought forward when a live conversion says so, and refused in
    // words when none does.
    const session_persist::LoadedSession last =
        session_persist::load_file(host_->session_path, host_->conversions);
    if (!last.present) {
        // A FIRST LAUNCH IS NOT AN ERROR and must never be reported as one. It is also
        // the most common way this function ends, so it ends quietly.
        return;
    }
    if (!last.outcome.accepted) {
        // And this run will not write over it: the session is a file Workshop writes on its way
        // out, so an orderly close would replace bytes this run could not read. The likeliest
        // cause is a conversion not mounted in this arrangement, which a maker fixes with a plan
        // row, on a file that has to still be there.
        session_refused_ = true;
        say(last.outcome.refusal + " -- opening with the default setup", true);
        // The notice is the event; the condition is what stays true all run, with a maker action
        // (`kSessionWallKey`).
        session_.conditions.establish(
            Condition{kSessionWallKey, "session refused -- this run keeps no session",
                      last.outcome.refusal +
                          " (the file is left exactly as it is, and this run will not "
                          "write over it)",
                      surface::role::kAlert, std::string()});
        return;
    }
    // ---- The viewport first, and the order is the whole of it ------------
    // `apply_setup` seats panes against the screen's capacity, so resizing after reconciling would
    // leave panes waiting for room that was there all along. The medium's own facts (the face
    // metric, the device unit) are handed back unchanged: a restore replaces only the room.
    if (last.honoured && adopt_screen(session_, last.viewport_w, last.viewport_h,
                                      session_.text_advance_px, session_.text_line_px,
                                      session_.cell_px)) {
        // The restored viewport IS the normal window's room -- the save wrote it from
        // exactly that -- so the remembered pair starts equal to it rather
        // than waiting for the first extent to arrive.
        session_.normal_w = session_.screen_w;
        session_.normal_h = session_.screen_h;
    }
    // ---- The desktop placement, remembered and offered back --------------------
    // Remembered first, so the next save carries it whether or not a medium acts on it, then
    // offered to the medium holding the surface: a want, not an instruction, since only the medium
    // sees the displays (`surface::SurfacePlacementRemembered`, `placement_within`).
    if (last.placement.known) {
        session_.placement_known = true;
        session_.place_x = last.placement.x;
        session_.place_y = last.placement.y;
        session_.place_maximized = last.placement.maximized;
        mail.send_to_role(zengine::surface::kSkinRole,
                          zengine::surface::SurfacePlacementRemembered{
                              last.placement.x, last.placement.y,
                              last.placement.maximized});
    }
    // ---- ...and then the desks, into the room they asked for ---------------
    // The whole run comes back and exactly one is lifted live (`install_layout_run`, the inverse
    // of `layout_run`); the others are values nothing opens or reconciles. It cannot refuse here:
    // `session_persist` proved the run non-empty and the position in range.
    install_layout_run(session_.setup, last.layouts, last.active);
    apply_setup(mail);
    // It says nothing about unresolved panes, unlike `restore_setup`: no provider has had a turn
    // yet, so every external reference is unresolved for a moment, and the setup line and the Pane
    // Manager name the genuinely gone ones live. It says how many layouts came back when more than
    // one did, counting from one, since it is prose about tabs.
    std::string said =
        "reopened your last desk " +
        quoted_setup_name(session_.setup.active.name);
    if (last.layouts.size() > 1) {
        said += " (" + std::to_string(last.active + 1) + " of " +
                std::to_string(last.layouts.size()) + " layouts)";
    }
    said += " -- " + std::to_string(session_.screen_w) + "x" +
            std::to_string(session_.screen_h) + " cells";
    if (!last.declined.empty()) {
        // AND IT NEVER CLAIMS THE SIZE CAME BACK WHEN IT DID NOT. The desk did; the
        // window did not; a maker is told which, with the value that was declined.
        said += "; " + last.declined;
    }
    // ...and once, if a pane in it changed hands (`workshop/pane_migration.hpp`): the count is the
    // whole run's.
    if (last.converted.total() > 0) {
        said += "; " + pane_migration::converted_note(last.converted);
    }
    say(said, !last.declined.empty());
    // THE SECOND PICTURE OF THE RUN, and the one that asks for the room -- see
    // `on(SurfaceReady)` for why it cannot be the first.
    repaint(mail);
}

// WL-SESSION-13, WL-SESSION-15 -- agents/workshop/session.md; WL-SESSION-16 -- agents/workshop/session-restore.md
// WL-MIG-10 -- agents/workshop/migration.md
void WorkshopWeave::save_last_session() {
    if (host_->session_path.empty() || session_refused_) {
        return;
    }
    // The viewport written is the normal window's, so a maximized close remembers the room the
    // maker chose, with the maximized state beside it. The placement rides along as last reported,
    // or as the file carried it.
    session_persist::Placement place;
    place.known = session_.placement_known;
    place.x = session_.place_x;
    place.y = session_.place_y;
    place.maximized = session_.place_maximized;
    // The whole run in maker order, with the position they stand on: `layout_run` answers with a
    // new vector, so saving cannot reorder what it saves.
    const Written written = session_persist::save_file(
        host_->session_path, layout_run(session_.setup), session_.setup.active_at,
        session_.normal_w, session_.normal_h, place);
    if (written.accepted) {
        return;
    }
    // Where a failure goes when the screen is what is leaving: the notice, because nothing fails
    // silently and a suite reads it, and stderr, because the maker will never see that notice.
    say(written.refusal, true);
    std::fprintf(stderr, "zengine-workshop: %s\n", written.refusal.c_str());
    std::fflush(stderr);
}

} // namespace zengine::workshop
