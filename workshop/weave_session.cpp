// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s sections -- the setup, the layout shelf and the last session --
// compiled once into `zengine-workshop-logic` and linked by the host and every suite; the
// declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/layouts.md (+7 registers; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- The setup: name it, save it, restore it ------------------------------

// WL-LAYOUT-05, WL-LAYOUT-07 -- agents/workshop/layouts.md
// WL-ARR-03 -- agents/workshop/arrangement.md
// WL-MAKER-09 -- agents/workshop/maker-pane.md
// WL-PED-05 -- agents/workshop/pane-manager.md
// WL-SESSION-12 -- agents/workshop/session.md
void WorkshopWeave::apply_setup(loom::Mail& mail) {
    // MEMBERSHIP-DEPENDENT SESSION STATE FIRST. This is the one door a setup's
    // membership changes through -- the picker's removal, a restore, a geometry edit
    // that reseats -- so it is the one place that has to notice a selection whose pane
    // is no longer named. Doing it here rather than at each caller is what keeps a
    // fourth caller from being the one that forgets.
    forget_removed_selection();
    const Reconciled done = reconcile(session_.panels, session_.setup.active,
                                      stack_capacity(screen_of(session_)));
    for (const std::int64_t kind : done.opened) {
        if (kind == panel::kBuilder) {
            (void)mail.send_to_role(zengine::builder::kBuilderRole,
                                    zengine::builder::StatusRequested{});
        } else if (kind == panel::kProjectFiles) {
            // A BROWSER THAT HAS JUST BECOME PRESENT LOOKS. This is the Builder's own
            // arm one kind over -- a pane that opens asks its subject what is true now
            // -- and it is where "the listing is a snapshot" becomes usable: reopening
            // the pane is a maker's way of asking for a fresh one, and it costs the
            // same walk as pressing refresh.
            files_refresh();
        }
    }
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
    // The line's own vocabulary first — the third of the four switches the
    // component call collapsed. What stays is the policy pair every consumer keeps to
    // itself: what a committed name MEANS and what abandoning one leaves standing.
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
    // NO `apply_setup`. A name is the one authored field of a desk that no
    // presentation reads: which panes participate, where they are and how big
    // they are have not moved, so there is nothing to reconcile. What DOES
    // change is the row this layout is painted on and, where the layout is
    // associated, whether it still matches its artifact -- both derived at the
    // next composition, from the value that just moved.
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
    session_.setup.active_link.path = path;
    adopt_known_setup(session_.setup, path, loaded.setup);
    apply_setup(mail);
    say("restored setup " + quoted_setup_name(loaded.setup.name) + " from " + path +
            unresolved_note(loaded.setup),
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

// WL-SESSION-11, WL-SESSION-12, WL-SESSION-14, WL-SESSION-16, WL-SESSION-17 -- agents/workshop/session.md
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
    //...AND WHATEVER CONVERSIONS THIS RUN HAPPENS TO HAVE, which is a reading
    // taken at this instant and not a capability this weave holds: an older session file
    // is brought forward exactly when a live conversion says so, by the same catalog
    // that answers every other operator question in this process, and is refused in
    // words when nothing does.
    const session_persist::LoadedSession last =
        session_persist::load_file(host_->session_path, host_->conversions);
    if (!last.present) {
        // A FIRST LAUNCH IS NOT AN ERROR and must never be reported as one. It is also
        // the most common way this function ends, so it ends quietly.
        return;
    }
    if (!last.outcome.accepted) {
        // ⚠ AND THIS RUN WILL NOT WRITE OVER IT, which is the marks file's own
        // law taken for a sharper reason. Restraint on the READ path was always here --
        // Workshop does not rewrite a file it could not understand -- but the session is
        // a file Workshop WRITES on its way out, so without this flag an orderly close
        // would replace bytes this run could not read with this run's default desk.
        //
        // IT COSTS ALMOST NOTHING AND IT BUYS BACK A WHOLE VINTAGE. the most
        // likely reason a session is refused is that the conversion for it is not mounted
        // in THIS arrangement -- a condition a maker fixes by adding a row to a plan, in
        // a minute, on a file that has to still be there when they do.
        session_refused_ = true;
        say(last.outcome.refusal + " -- opening with the default setup", true);
        // THE NOTICE IS THE EVENT; THE CONDITION IS WHAT IS STILL TRUE. The sentence
        // above is about this launch and the next thing said replaces it; that this
        // Workshop is keeping no session, over a file that is still on disk, is true all
        // run and has a maker action -- which is what makes it a condition
        // (`kSessionWallKey`, the keymap/prefs/marks walls' own shape).
        session_.conditions.establish(
            Condition{kSessionWallKey, "session refused -- this run keeps no session",
                      last.outcome.refusal +
                          " (the file is left exactly as it is, and this run will not "
                          "write over it)",
                      surface::role::kAlert, std::string()});
        return;
    }
    // ---- THE VIEWPORT FIRST, AND THE ORDER IS THE WHOLE OF IT ------------
    //
    // `apply_setup` seats panes against `stack_capacity(screen_of(session_))`, so how
    // much of this desk can be PRESENTED at all is decided by how much room the screen
    // has. Reconciling first and resizing afterwards would seat the desk against a
    // viewport nobody asked for and leave whatever did not fit waiting for room that had
    // in fact been there the whole time.
    // THE MEDIUM'S OWN FACTS ARE HANDED BACK UNCHANGED -- the face metric and
    // the canvas's device unit. A restore replaces the ROOM, which is the
    // only thing the file remembers; what the medium said about its own units is this
    // run's and must survive the call rather than be reset to the character reading.
    if (last.honoured && adopt_screen(session_, last.viewport_w, last.viewport_h,
                                      session_.text_advance_px, session_.text_line_px,
                                      session_.cell_px)) {
        // The restored viewport IS the normal window's room -- the save wrote it from
        // exactly that -- so the remembered pair starts equal to it rather
        // than waiting for the first extent to arrive.
        session_.normal_w = session_.screen_w;
        session_.normal_h = session_.screen_h;
        // The resolved inspector row closes over the workspace extent, and the workspace
        // extent is exactly what just changed -- `on(SurfaceExtent)`'s reason, said at
        // startup.
        refocus_keeping_draft(state_, session_);
    }
    // ---- THE DESKTOP PLACEMENT, REMEMBERED AND OFFERED BACK --------------------
    //
    // Remembered FIRST -- into the session, so the next save carries it whether or not
    // any medium ever acts on it (a terminal run retains a graphical run's placement
    // rather than erasing it) -- and then OFFERED to whichever medium holds the
    // surface. The offer is a want, not an instruction: the medium can see the
    // displays that exist now and Workshop cannot, so the judgment (restore verbatim,
    // adapt a stranded position, refuse to move blind) is entirely the medium's
    // (`surface::SurfacePlacementRemembered`; the law is `placement_within`). What the
    // medium then reports back through the ordinary placement channel is the truth
    // this session remembers next.
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
    // ---- ...AND THEN THE DESKS, INTO THE ROOM THEY ASKED FOR ---------------
    //
    // THE WHOLE RUN COMES BACK AND EXACTLY ONE OF IT IS LIFTED LIVE.
    // `install_layout_run` is `layout_run`'s inverse and lives beside it in
    // `setup.hpp`, so this weave never touches `shelved` or `active_at` by hand and
    // there is no second spelling of the lift to drift. The layouts that are not live
    // are VALUES: no panel is opened for one, no provider hears about one, and nothing
    // is reconciled against one -- which is why `apply_setup` below is still the one
    // membership door and still sees exactly one desk.
    //
    // IT CANNOT REFUSE HERE, and the reason is where the law is: an admitted session's
    // run is non-empty and its position is in range, because `session_persist` proved
    // both before this value existed. The bool is the TYPE's floor for callers that
    // have not.
    install_layout_run(session_.setup, last.layouts, last.active);
    apply_setup(mail);
    // AND IT SAYS NOTHING ABOUT UNRESOLVED PANES, WHICH `restore_setup` DOES SAY.
    //
    // MEASURED, ON A REAL WINDOW: at this instant no provider has had a turn. Workshop
    // published `PaneCatalogRequested` a few lines ago and the answers are still in the
    // queue, so EVERY external reference in a restored desk is unresolved right now and
    // resolved a moment later -- the count is a fact about the clock rather than about
    // the desk, and a maker reads it after it has stopped being true. Measured: a desk of
    // three external panes reported all three unresolved, BY NAME, over three panes that
    // were on the screen while the sentence was being read.
    //
    // IT IS NOT A LOST DIAGNOSTIC. The setup line carries the same count LIVE and
    // recomputes it every paint (`setup "..." UNSAVED [| N unresolved]`), and the picker
    // gives an unresolved reference a row of its own. A reference that is genuinely gone
    // is therefore still named, by a surface that is still right an hour later. `r` keeps
    // its note, because a maker who presses it is asking a question at a moment when the
    // catalog has long since been answered.
    // AND IT SAYS HOW MANY CAME BACK WHEN MORE THAN ONE DID. One layout is
    // the sentence this has always been and every migrated session is one, so the
    // clause appears exactly when there is more to say. It COUNTS FROM ONE because it
    // is prose about tabs a maker is looking at; the file's own `active` is a position
    // and is spoken from zero where a maker is reading the file (`session_persist`'s
    // refusals).
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
    say(said, !last.declined.empty());
    // THE SECOND PICTURE OF THE RUN, and the one that asks for the room -- see
    // `on(SurfaceReady)` for why it cannot be the first.
    repaint(mail);
}

// WL-SESSION-13, WL-SESSION-15, WL-SESSION-16 -- agents/workshop/session.md
// WL-MIG-10 -- agents/workshop/migration.md
void WorkshopWeave::save_last_session() {
    if (host_->session_path.empty() || session_refused_) {
        return;
    }
    // THE VIEWPORT WRITTEN IS THE NORMAL WINDOW'S: `normal_w/h` tracks the
    // screen except while this run's medium says the window is maximized, so a
    // maximized close remembers the room a maker actually chose, with the maximized
    // state beside it rather than baked into it. The placement rides along exactly as
    // the last medium reported it -- or exactly as the file already carried it, on a
    // run whose medium had no desktop to report.
    session_persist::Placement place;
    place.known = session_.placement_known;
    place.x = session_.place_x;
    place.y = session_.place_y;
    place.maximized = session_.place_maximized;
    // THE WHOLE RUN, IN MAKER ORDER, WITH THE POSITION THEY ARE ACTUALLY STANDING IN
    //. `layout_run` puts the lifted value back where it sits and answers with
    // a NEW vector, so saving cannot reorder what it is saving; and the position is
    // `active_at` rather than the end of the run, because `layout.new` leaving a maker
    // on the last layout is a habit and not a law.
    const Written written = session_persist::save_file(
        host_->session_path, layout_run(session_.setup), session_.setup.active_at,
        session_.normal_w, session_.normal_h, place);
    if (written.accepted) {
        return;
    }
    // WHERE A FAILURE GOES WHEN THE SCREEN IS THE THING LEAVING. The notice is set
    // because nothing here may fail silently and because a suite reads it; stderr is
    // written because a maker will never see that notice -- this runs on the way out,
    // and a complaint about the session, delivered to a surface that is being torn down,
    // is not a complaint. The same argument the SDL medium makes for `complain`.
    say(written.refusal, true);
    std::fprintf(stderr, "zengine-workshop: %s\n", written.refusal.c_str());
    std::fflush(stderr);
}

} // namespace zengine::workshop
