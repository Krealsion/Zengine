// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `weave.hpp`'s desktop seam -- the application-defaults owner's declaration, the
// launch door, the one inventory said out loud, and the floor of the empty room. Compiled once
// into `zengine-workshop-logic` and linked by the host and every suite.
// Workshop law: agents/workshop/desktop.md (agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- Telling a declarer that its declaration was refused (BL-WORK-04) --------------------

void WorkshopWeave::say_actions_refused(const std::string& office, const std::string& pane,
                                        const std::string& refusal, loom::Mail& mail) {
    // THE MAKER FIRST, because the band is where a refusal has always been said and a
    // provider's recovery is not a substitute for a maker knowing their key did not move.
    say(refusal, true);
    if (office.empty()) {
        return;
    }
    // ...AND THE DECLARER, at the office Loom stamped on the declaration. Addressed rather
    // than published: this is a fact about ONE party's declaration, and a broadcast would tell
    // every listening weave which gestures another provider tried to take.
    //
    // ⚠ NOTHING IS RETRIED AND NOTHING IS WAITED FOR. If the send itself is refused at Loom's
    // gate -- a provider that never declared it accepts this shape -- the refusal stands
    // exactly as it does now, said on the band. Workshop owes the attempt, not the arrival.
    (void)mail.as_role(kWorkshopProvider).send_to_role(office, ActionsRefused{pane, refusal});
}

// ---- The application's declared defaults --------------------------------------------------

// WL-DESK-07 -- agents/workshop/desktop.md
void WorkshopWeave::on(const AppActions& actions, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // personal speech declares nothing -- the offer's rule, one seam over
    }
    // ⚠ ONE OFFICE OWNS THE APPLICATION'S DEFAULTS, AND IT IS NAMED. This is not a password:
    // `kDesktopRole` is an address, and who may hold it is the host policy's answer (the
    // Weaver, set by the maker -- VD-21). What the test forbids is TWO parties owning "what
    // this application answers to above every mode", which is the duplicate-authority defect
    // the seam exists to end. A second office is refused BY NAME and told so.
    if (office != kDesktopRole) {
        say_actions_refused(std::string(office), std::string(),
                            "application actions are declared by `" +
                                std::string(kDesktopRole) + "` -- `" + std::string(office) +
                                "` declares its own pane's rows instead",
                            mail);
        repaint(mail);
        return;
    }
    if (actions.rows.size() > kMaxAppActionRows) {
        say_actions_refused(std::string(office), std::string(),
                            "an application declares at most " +
                                std::to_string(kMaxAppActionRows) + " actions -- this one "
                                "declared " + std::to_string(actions.rows.size()),
                            mail);
        repaint(mail);
        return;
    }
    std::vector<AppRow> rows;
    rows.reserve(actions.rows.size());
    for (const AppActionRow& d : actions.rows) {
        rows.push_back(AppRow{d.id, d.label, Gesture{d.scancode, d.modifiers}, d.precedence});
    }
    // JUDGED INTO A COPY, so a refusal anywhere leaves the rows in force exactly as they
    // were -- `declare_pane_actions`' discipline, one scope out.
    Keymap candidate = session_.keymap;
    const Written joined = join_app_rows(candidate, rows);
    if (!joined.accepted) {
        say_actions_refused(std::string(office), std::string(), joined.refusal, mail);
        repaint(mail);
        return;
    }
    session_.keymap = std::move(candidate);
    // AND EVERY PANE IS JOINED AGAIN UNDER THE ROWS NOW IN FORCE. A pane that declared
    // before the desktop did was judged against a map without these rows in it; leaving it
    // there would let one gesture mean two things depending on which weave loaded first.
    // A pane whose rows the new application rows take is the party judged, told, and left
    // with none -- both load orders end the same way (WL-DESK-07).
    std::string refusals;
    rejoin_pane_rows(refusals, mail);
    app_actions_ = actions.rows;
    if (!refusals.empty()) {
        say(refusals, true);
    }
    repaint(mail);
}

// ---- Asking the desktop for one of its rows, and hearing back -----------------------------

// WL-DESK-02 -- agents/workshop/desktop.md
void WorkshopWeave::request_app_action(const std::string& id, loom::Mail& mail) {
    // THIS KEYSTROKE'S OWN NUMBER, minted here and nowhere else. Monotonic from one, so zero
    // is never an ask: an answer that echoes nothing answers nothing (`escape_asks_`'s rule,
    // one owner over).
    const std::uint64_t answering = ++app_asks_;
    app_asked_ = AppAsked{gestures_, answering};
    (void)mail.as_role(kWorkshopProvider)
        .send_to_role(kDesktopRole, AppActionRequested{id}, answering);
}

// WL-DESK-02 -- agents/workshop/desktop.md
void WorkshopWeave::on(const DeselectRequested&, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty() || office != kDesktopRole) {
        return;
    }
    // THE PARTICULAR ASK THIS ANSWERS, FIRST -- `on(PaneEscapeUnspent)`'s repair, preserved
    // through the relocation. Matching current state cannot identify the event being answered:
    // a second Escape leaves the maker with a selection and a latest gesture all over again,
    // and the first press's answer would be spent on the second's record.
    if (mail.correlation() == 0 || mail.correlation() != app_asked_.answering) {
        return;
    }
    // ...AND IT IS STILL THE MAKER'S LATEST GESTURE. A key, text, press or wheel since leaves
    // this about a keystroke that is no longer what the maker did last.
    if (app_asked_.gesture != gestures_) {
        return;
    }
    app_asked_ = AppAsked{};
    if (session_.panels.selected == kNoPaneKind) {
        return; // nothing is picked up; putting nothing down says nothing
    }
    unselect_pane();
    repaint(mail);
}

// ---- Launching ----------------------------------------------------------------------------

// WL-DESK-03 -- agents/workshop/desktop.md
PaneLaunchAnswered WorkshopWeave::launch_pane(const PaneRef& ref, loom::Mail& mail) {
    PaneLaunchAnswered out;
    out.office = ref.provider;
    out.pane = ref.pane;
    // THE ONE INVENTORY ANSWERS, AND NOTHING ELSE DOES. `inventory_rows` is the population the
    // launcher lists, the Pane Manager manages and a restore resolves against; asking anything
    // else here would be the second inventory this arc exists to not create.
    const std::vector<CatalogRow> rows = inventory_rows(session_.setup.active, session_.panels);
    const CatalogRow* found = nullptr;
    for (const CatalogRow& row : rows) {
        if (row.ref == ref) {
            found = &row;
            break;
        }
    }
    if (found == nullptr) {
        // ⚠ AND THIS IS WHERE A LAUNCH DOES NOT LOAD ANYTHING. A pane no provider has offered
        // and no desk has authored is not a pane this Workshop can open; going and finding an
        // artifact for it would be a launch request granting itself the authority a plan row
        // asks for (VD-21). The refusal says which name was asked for, because a maker whose
        // keymap names a pane that moved needs to see the spelling.
        out.refusal = "no pane `" + ref.pane + "` from `" + ref.provider +
                      "` -- nothing has offered it, and a launch loads nothing";
        return out;
    }
    // COPIED BEFORE THE DESK MOVES: `apply_setup` may re-ask providers and grow the catalog
    // vector under a pointer into it (panel.hpp's own warning).
    const std::int64_t kind = found->kind;
    const std::string name = found->name;
    if (kind == kNoPaneKind) {
        // ⚠ CLOSED PARTICIPATION AND AN ABSENT PROVIDER ARE DIFFERENT ANSWERS. A row with no
        // kind is one a DESK authored and no office resolves -- the tool is unavailable, not
        // closed -- and saying "opened" for it would be the presentation claiming a seat it
        // does not have. This is the sentence the desktop's backdrop turns into an
        // explanation a maker can act on.
        out.refusal = name + " is not available -- `" + ref.provider +
                      "` is not offering it in this Workshop";
        return out;
    }
    const bool already = session_.panels.has(kind);
    if (!already) {
        // JUDGED THROUGH THE PICKER'S OWN TRIAL SEAT, on a copy, before the setup moves --
        // `on(PaneRevealRequested)`'s discipline, which exists so that a refusal never leaves
        // a maker with an authored pane they never saw.
        Setup candidate = session_.setup.active;
        const bool added = add_pane(candidate, ref);
        const Seating trial =
            seat_panes(candidate, session_.panels, stack_capacity(screen_of(session_)));
        for (const std::int64_t k : trial.waiting) {
            if (k == kind) {
                out.refusal = "no room for " + name +
                              " on this screen -- make the window taller, then try again";
                return out;
            }
        }
        if (added) {
            session_.setup.active = std::move(candidate);
        }
        apply_setup(mail);
        if (!session_.panels.has(kind)) {
            // THE BELT UNDER THE TRIAL, and the same take-back: "opened" is the word the
            // asker acts on and it may never be said of a pane the screen does not show.
            if (added) {
                (void)remove_pane(session_.setup.active, ref);
                apply_setup(mail);
            }
            out.refusal = "no room for " + name + " on this screen";
            return out;
        }
        out.opened = true;
    }
    // ⚠ AND IT FOCUSES EITHER WAY, WHICH IS THE WHOLE DIFFERENCE FROM THE PICKER'S TOGGLE.
    // A launch of a pane already on the desk puts the maker IN it: selected, and holding the
    // keys if it takes them. It does not close it, does not take its row out of the setup and
    // does not unload its provider -- a maker who presses the Terminal key while the Terminal
    // is open wanted the Terminal.
    session_.panels.selected = kind;
    session_.panels.keyboard = kind_takes_keyboard(kind) ? kind : kNoPaneKind;
    out.focused = true;
    return out;
}

// WL-DESK-03 -- agents/workshop/desktop.md
void WorkshopWeave::on(const PaneLaunchRequested& asked, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // an office, and only an office -- the seam's rule for changing the desk
    }
    const PaneLaunchAnswered answer = launch_pane(PaneRef{asked.office, asked.pane}, mail);
    // SAID ON THE BAND FOR THE MAKER, AND ANSWERED TO THE ASKER. The two say the same thing
    // for the two readers, and both are said before the repaint, so the picture a maker sees
    // is the one the sentence is about.
    if (!answer.refusal.empty()) {
        say(answer.refusal, true);
    } else if (answer.opened) {
        say("opened " + asked.pane, false);
    }
    (void)mail.answer(answer);
    repaint(mail);
}

// ---- The one inventory, said out loud ------------------------------------------------------

// WL-DESK-04 -- agents/workshop/desktop.md
void WorkshopWeave::publish_inventory(loom::Mail& mail) {
    PaneInventory said;
    const Seating seated =
        seat_panes(session_.setup.active, session_.panels, stack_capacity(screen_of(session_)));
    for (const CatalogRow& row : inventory_rows(session_.setup.active, session_.panels)) {
        InventoryPane p;
        p.office = row.ref.provider;
        p.pane = row.ref.pane;
        p.name = row.name;
        p.summary = row.summary;
        // THE THREE FACTS ARE ASKED OF THREE OWNERS, and keeping them apart is the point of
        // the shape. Authored participation is the SETUP's; whether anything offers the pane
        // is the CATALOG's (a row with no kind resolves to no provider); whether this screen
        // could seat it is the SEATING's. A presentation that collapsed them would have to
        // guess which of "closed", "gone" and "no room" a missing pane is in.
        p.open = has_pane(session_.setup.active, row.ref);
        p.available = row.kind != kNoPaneKind;
        for (const std::int64_t k : seated.waiting) {
            if (k == row.kind) {
                p.waiting = true;
                break;
            }
        }
        said.panes.push_back(std::move(p));
    }
    // COMPARED BEFORE IT IS PUBLISHED, for `DocumentShown`'s reason: a presenter told the same
    // thing twice repaints for nothing, and the inventory is derived at every gesture.
    if (said.panes.size() == inventory_said_.size()) {
        bool same = true;
        for (std::size_t i = 0; i < said.panes.size(); ++i) {
            const InventoryPane& a = said.panes[i];
            const InventoryPane& b = inventory_said_[i];
            if (a.office != b.office || a.pane != b.pane || a.name != b.name ||
                a.summary != b.summary || a.open != b.open || a.available != b.available ||
                a.waiting != b.waiting) {
                same = false;
                break;
            }
        }
        if (same) {
            return;
        }
    }
    inventory_said_ = said.panes;
    mail.publish(said);
}

// ---- The floor of the empty room ------------------------------------------------------------

// WL-DESK-05 -- agents/workshop/desktop.md
void WorkshopWeave::on(const DesktopFace& face, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty() || office != kDesktopRole) {
        return; // the room's floor is the desktop office's to say, and nobody else's
    }
    if (face.rows.size() > kMaxBackdropRows) {
        // REFUSED WHOLE RATHER THAN TRUNCATED, for `PaneContent`'s reason: a floor showing
        // an unmarked part of what the desktop said would be this host presenting a partial
        // sentence as a complete answer.
        say("the desktop said " + std::to_string(face.rows.size()) + " rows; the room's floor "
            "holds " + std::to_string(kMaxBackdropRows),
            true);
        repaint(mail);
        return;
    }
    session_.backdrop = face.rows;
    repaint(mail);
}

} // namespace zengine::workshop
