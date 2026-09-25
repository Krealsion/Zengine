// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s desktop seam: the defaults owner's declaration, the launch door, the one
// inventory said out loud, and the floor of the empty room.
// Workshop law: agents/workshop/desktop.md (agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// ---- A declarer's verdicts --------------------------------------------------------------

// WL-DESK-06 -- agents/workshop/desktop.md
void WorkshopWeave::answer_declaration(const std::string& office, ActionsJudged verdict,
                                       loom::Mail& mail) {
    // THE MAKER FIRST, for a refusal, because the band is where a refusal has always been said
    // and a provider's recovery is not a substitute for a maker knowing their key did not move.
    if (!verdict.accepted) {
        say(verdict.refusal, true);
    }
    // ...and the declarer, as the answer to its declaration: Loom carries the declaration's own
    // correlation back to the incarnation that sent it (ANS-03), so a verdict cannot land on a
    // successor after a reload. Only a holder that reads the shape is answered, and nothing is
    // retried; an older provider is told nothing, which is not acceptance.
    if (office.empty() || !host_->holder_accepts ||
        !host_->holder_accepts(office, *loom::schema_of<ActionsJudged>())) {
        return;
    }
    (void)mail.answer(verdict);
}

// WL-DESK-06 -- agents/workshop/desktop.md
void WorkshopWeave::say_withdrawn(const std::string& office, ActionsWithdrawn withdrawn,
                                  loom::Mail& mail) {
    // ADDRESSED, NOT PUBLISHED: a fact about ONE party's declaration. Ordinary speech follows the
    // office, so the number is what tells a successor that this was its predecessor's rows.
    if (office.empty() || withdrawn.declaration == 0 || !host_->holder_accepts ||
        !host_->holder_accepts(office, *loom::schema_of<ActionsWithdrawn>())) {
        return;
    }
    (void)mail.as_role(kWorkshopProvider).send_to_role(office, withdrawn);
}

// ---- The application's declared defaults --------------------------------------------------

// WL-DESK-07 -- agents/workshop/desktop.md
void WorkshopWeave::on(const AppActions& actions, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // personal speech declares nothing -- the offer's rule, one seam over
    }
    // One office owns the application's defaults, and it is named: `kDesktopRole` is an address,
    // and who may hold it is host policy. Two owners of "what answers above every mode" is the
    // defect this seam ends, so a second office is refused by name.
    if (office != kDesktopRole) {
        answer_declaration(std::string(office),
                           ActionsJudged{std::string(), false, 0,
                                         "application actions are declared by `" +
                                             std::string(kDesktopRole) + "` -- `" +
                                             std::string(office) +
                                             "` declares its own pane's rows instead"},
                           mail);
        repaint(mail);
        return;
    }
    if (actions.rows.size() > kMaxAppActionRows) {
        answer_declaration(std::string(office),
                           ActionsJudged{std::string(), false, 0,
                                         "an application declares at most " +
                                             std::to_string(kMaxAppActionRows) +
                                             " actions -- this one declared " +
                                             std::to_string(actions.rows.size())},
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
        answer_declaration(std::string(office),
                           ActionsJudged{std::string(), false, 0, joined.refusal}, mail);
        repaint(mail);
        return;
    }
    session_.keymap = std::move(candidate);
    app_declaration_ = ++declarations_;
    answer_declaration(std::string(office),
                       ActionsJudged{std::string(), true, app_declaration_, std::string()}, mail);
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

// WL-DESK-06 -- agents/workshop/desktop.md
void WorkshopWeave::rejoin_app_rows(std::string& refusals, loom::Mail& mail) {
    if (app_actions_.empty()) {
        return;
    }
    std::vector<AppRow> rows;
    rows.reserve(app_actions_.size());
    for (const AppActionRow& d : app_actions_) {
        rows.push_back(AppRow{d.id, d.label, Gesture{d.scancode, d.modifiers}, d.precedence});
    }
    Keymap candidate = session_.keymap;
    const Written joined = join_app_rows(candidate, rows);
    if (joined.accepted) {
        session_.keymap = std::move(candidate);
        return;
    }
    // THE FILE WINS, and the declaration it displaced is WITHDRAWN rather than kept: kept, it
    // would come back into force at some later join without its declarer being told.
    if (!refusals.empty()) {
        refusals += "; ";
    }
    refusals += "the application's keys: " + joined.refusal;
    const std::int64_t was = app_declaration_;
    app_actions_.clear();
    app_declaration_ = 0;
    say_withdrawn(std::string(kDesktopRole),
                  ActionsWithdrawn{std::string(), was, "the application's keys: " + joined.refusal},
                  mail);
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
    // The particular ask this answers, first: current state cannot identify it, since a second
    // Escape recreates that state.
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
    // The one inventory answers, and nothing else: the population the Pane Manager lists, both
    // doors judge and a restore resolves against.
    const std::vector<CatalogRow> rows = inventory_rows(session_.setup.active, session_.panels);
    const CatalogRow* found = nullptr;
    for (const CatalogRow& row : rows) {
        if (row.ref == ref) {
            found = &row;
            break;
        }
    }
    // ⚠ STILL TO COME IS NOT A REFUSAL'S VERDICT. While the plan row loading the office has not
    // settled, nothing offers the pane YET, and "not available -- build it, or relaunch" would be
    // a verdict the run has not reached. The launch is still refused (a launch loads nothing),
    // in words that say so.
    const bool pending = host_->office_pending && host_->office_pending(ref.provider);
    const auto not_yet = [&ref](const std::string& who) {
        return who + " is not here yet -- this run is still loading `" + ref.provider +
               "`; launch it again once it has arrived";
    };
    if (found == nullptr && pending) {
        out.refusal = not_yet("`" + ref.pane + "`");
        return out;
    }
    if (found == nullptr) {
        // A launch loads nothing: a pane no provider offered and no desk authored is no pane this
        // Workshop can open, and finding an artifact for it would grant a launch a plan row's
        // authority. The refusal spells the name asked for.
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
        out.refusal = pending ? not_yet(name)
                              : name + " is not available -- `" + ref.provider +
                                    "` is not offering it in this Workshop";
        return out;
    }
    if (!provider_present(kind, ref)) {
        // ⚠ A PAST OFFER IS NOT A PRESENT PROVIDER. The catalog keeps the row (its identity and
        // the desk rows that name it), but nobody holds the office now, so nobody could fill
        // the pane: it would open onto a room nothing answers. Asked of the bus at this
        // instant; presence is all it proves -- not health, and not that a delivery lands.
        out.refusal = pending ? not_yet(name)
                              : name + " is not available -- nothing holds `" + ref.provider +
                                    "` now; its artifact has to be loaded again (build it, or "
                                    "relaunch)";
        return out;
    }
    const bool already = session_.panels.has(kind);
    if (!already) {
        // Judged through the trial seat (`seat_panes`), on a copy, before the setup moves, so a
        // refusal never leaves the maker an authored pane they never saw.
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
    // And it focuses either way: a launch of a pane already on the desk puts the maker in it,
    // selected and holding the keys if it takes them, and never closes it.
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
        say("opened " + inventory_name(PaneRef{asked.office, asked.pane}), false);
    }
    (void)mail.answer(answer);
    repaint(mail);
}

// ---- Closing: participation, and nothing behind it ----------------------------------------

std::string WorkshopWeave::inventory_name(const PaneRef& ref) const {
    for (const CatalogRow& row : inventory_rows(session_.setup.active, session_.panels)) {
        if (row.ref == ref) {
            return row.name;
        }
    }
    return ref.pane;
}

// WL-DESK-12 -- agents/workshop/desktop.md
PaneCloseAnswered WorkshopWeave::close_pane(const PaneRef& ref, loom::Mail& mail) {
    PaneCloseAnswered out;
    out.office = ref.provider;
    out.pane = ref.pane;
    // THE DESK IS WHAT IS ASKED, NOT THE CATALOG: a row the maker authored is theirs to take off
    // whether or not anything offers it -- an unavailable pane's row, or one waiting for room, is
    // exactly the intent a maker closes to stop asking for it.
    if (!remove_pane(session_.setup.active, ref)) {
        out.refusal = inventory_name(ref) + " is not on this desk -- nothing to close, and a "
                                            "close opens nothing";
        return out;
    }
    // And nothing behind it is touched: the presentation leaves with the row; the office's
    // holder, its state and its outstanding asks stay, since participation is the desk's fact and
    // a provider's lifetime is realization's.
    apply_setup(mail);
    out.closed = true;
    return out;
}

// WL-DESK-12 -- agents/workshop/desktop.md
void WorkshopWeave::on(const PaneCloseRequested& asked, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return; // an office, and only an office -- the seam's rule for changing the desk
    }
    const PaneRef ref{asked.office, asked.pane};
    const PaneCloseAnswered answer = close_pane(ref, mail);
    if (!answer.refusal.empty()) {
        say(answer.refusal, true);
    } else {
        say("closed " + inventory_name(ref) +
                " -- its provider and what it holds are untouched; launching it opens it again",
            false);
    }
    (void)mail.answer(answer);
    repaint(mail);
}

// ---- Editing a binding: the candidate judged whole, applied live, then written -------------

namespace {

/// A KEYMAP FILE-ROW SPELLING OF A GESTURE, `none` for an unbound one -- the same grammar a
/// maker writes, because an edit writes what a hand would have.
std::string authored_spelling(const Gesture& g) {
    return is_bound(g) ? gesture_word(g) : std::string("none");
}

} // namespace

// WL-KEY-17, WL-KEY-18 -- agents/workshop/keymap-edit.md
void WorkshopWeave::on(const KeymapEditRequested& asked, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return; // an office asks; personal speech is answered by nobody
    }
    KeymapEditAnswered answer;
    answer.action = asked.action;
    const auto refuse = [&](const std::string& why) {
        answer.accepted = false;
        answer.refusal = why;
        answer.sentence = "`" + asked.action + "` unchanged -- " + why;
        say(answer.sentence, true);
        (void)mail.answer(answer);
        repaint(mail);
    };
    // A FILE REFUSED AT LAUNCH IS A STANDING WALL, and an edit does not climb it: the host never
    // writes over a file the maker has not repaired, and a live change beside a refused file
    // would be a map the next launch does not have.
    if (keymap_bad_) {
        refuse("the keymap file was refused at launch (" + keymap_standing_ +
               ") -- fix the file, launch again, then edit");
        return;
    }
    // WHICH ID, AND ITS DECLARED DEFAULT: a host row, an application row, or a pane's row --
    // the one keymap's three populations, resolved against what is in force now.
    Gesture declared = kNoGesture;
    bool known = false;
    if (const ActionRow* host_row = row_of_id(asked.action)) {
        declared = host_row->gesture;
        known = true;
    }
    if (!known) {
        for (const AppActionRow& d : app_actions_) {
            if (d.id == asked.action) {
                declared = Gesture{d.scancode, d.modifiers};
                known = true;
                break;
            }
        }
    }
    if (!known) {
        for (const RuntimePane& row : session_.panels.runtime.entries) {
            for (const v2::PaneActionRow& d : row.actions) {
                if (d.id == asked.action) {
                    declared = Gesture{d.scancode, d.modifiers};
                    known = true;
                    break;
                }
            }
            if (known) {
                break;
            }
        }
    }
    if (!known) {
        refuse("no action `" + asked.action + "` is in force in this Workshop");
        return;
    }
    // THE GESTURE THE EDIT NAMES: two numbers this keymap can spell, or a spelling in the
    // file's own grammar for a chord no pane can capture.
    Gesture named = kNoGesture;
    const bool spelled = asked.op == keymap_edit::kSetSpelled ||
                         asked.op == keymap_edit::kAddSpelled ||
                         asked.op == keymap_edit::kRemoveSpelled;
    const bool takes_gesture = asked.op == keymap_edit::kSet || asked.op == keymap_edit::kAdd ||
                               asked.op == keymap_edit::kRemove || spelled;
    if (spelled) {
        const ParsedGesture parsed = parse_gesture(asked.text);
        if (!parsed.accepted) {
            refuse(parsed.refusal);
            return;
        }
        named = parsed.gesture;
    } else if (takes_gesture) {
        named = Gesture{asked.scancode, asked.modifiers};
        if (!is_bound(named) || key_name_of(asked.scancode) == nullptr) {
            refuse("that key is not one this keymap can name -- type its spelling instead");
            return;
        }
    }
    const std::int64_t op = asked.op == keymap_edit::kSetSpelled      ? keymap_edit::kSet
                            : asked.op == keymap_edit::kAddSpelled    ? keymap_edit::kAdd
                            : asked.op == keymap_edit::kRemoveSpelled ? keymap_edit::kRemove
                                                                      : asked.op;
    // THE SET IN FORCE FOR THIS ID: the authored rows if the file names it, the declared
    // default otherwise -- what `add` extends and `remove` subtracts from.
    std::vector<std::string> before;
    for (const AuthoredOverride& o : session_.keymap.authored) {
        if (o.action == asked.action) {
            before.push_back(o.gesture);
        }
    }
    const bool authored = !before.empty();
    if (!authored) {
        before.push_back(authored_spelling(declared));
    }
    std::vector<std::string> after;
    const std::string word = authored_spelling(named);
    switch (op) {
    case keymap_edit::kSet:
        if (!is_bound(named)) {
            refuse("`none` is not a key -- disable the action, or reset it");
            return;
        }
        after = {word};
        break;
    case keymap_edit::kAdd: {
        if (!is_bound(named)) {
            refuse("`none` is not a key to add");
            return;
        }
        for (const std::string& g : before) {
            if (g == word) {
                refuse("`" + word + "` already requests `" + asked.action + "`");
                return;
            }
        }
        for (const std::string& g : before) {
            if (g != "none") {
                after.push_back(g);
            }
        }
        after.push_back(word);
        break;
    }
    case keymap_edit::kRemove: {
        bool had = false;
        for (const std::string& g : before) {
            if (g == word) {
                had = true;
            } else {
                after.push_back(g);
            }
        }
        if (!had) {
            refuse("`" + word + "` does not request `" + asked.action + "` -- nothing to remove");
            return;
        }
        // REMOVING THE LAST KEY DISABLES THE ACTION, aloud -- never a silent fall-back to the
        // default the maker just took away; reset is the way back to it.
        if (after.empty()) {
            after = {"none"};
        }
        break;
    }
    case keymap_edit::kDisable: after = {"none"}; break;
    case keymap_edit::kReset: after.clear(); break;
    default: refuse("an edit this Workshop does not know"); return;
    }
    // THE CANDIDATE AUTHORED LIST: every other id's rows exactly as written, in their order;
    // this id's rows replaced in place (the first one's position) or appended.
    std::vector<std::pair<std::string, std::string>> rows;
    bool placed = false;
    for (const AuthoredOverride& o : session_.keymap.authored) {
        if (o.action != asked.action) {
            rows.emplace_back(o.action, o.gesture);
            continue;
        }
        if (!placed) {
            for (const std::string& g : after) {
                rows.emplace_back(asked.action, g);
            }
            placed = true;
        }
    }
    if (!placed) {
        for (const std::string& g : after) {
            rows.emplace_back(asked.action, g);
        }
    }
    // JUDGED EXACTLY AS A FILE IS JUDGED AT LOAD, into a copy: the override law over the host
    // rows, then the application rows and every pane's rows re-joined against the candidate. A
    // refusal anywhere is that law's own sentence, attributed to this edit, and nothing moves.
    Keymap candidate;
    const Written applied = apply_overrides(rows, session_.keymap.legend, candidate);
    if (!applied.accepted) {
        refuse(applied.refusal);
        return;
    }
    if (!app_actions_.empty()) {
        std::vector<AppRow> app;
        for (const AppActionRow& d : app_actions_) {
            app.push_back(AppRow{d.id, d.label, Gesture{d.scancode, d.modifiers}, d.precedence});
        }
        const Written joined = join_app_rows(candidate, app);
        if (!joined.accepted) {
            refuse("the application's keys: " + joined.refusal);
            return;
        }
    }
    for (const RuntimePane& row : session_.panels.runtime.entries) {
        if (row.actions.empty()) {
            continue;
        }
        const Written joined = join_pane_rows(candidate, row.kind, row.actions);
        if (!joined.accepted) {
            refuse(row.name + " @" + row.provider + ": " + joined.refusal);
            return;
        }
    }
    // APPLIED LIVE: the candidate is the map now, and every presenter is told (WL-DESK-11).
    session_.keymap = std::move(candidate);
    answer.accepted = true;
    answer.applied = true;
    std::string how;
    switch (op) {
    case keymap_edit::kSet: how = "now `" + word + "`"; break;
    case keymap_edit::kAdd: how = "also `" + word + "`"; break;
    case keymap_edit::kRemove:
        how = after.size() == 1 && after[0] == "none"
                  ? "`" + word + "` removed -- no key requests it now; reset restores the default"
                  : "`" + word + "` removed";
        break;
    case keymap_edit::kDisable: how = "disabled -- no key requests it; reset restores the default"; break;
    default: how = "reset to its default"; break;
    }
    // ...THEN WRITTEN, AND THE TWO TOLD APART. An isolated run has no file; a file another hand
    // changed since this host read it is never overwritten; a write that fails says so. In each
    // case the live change stands for this run only, and a standing condition says the next
    // launch will not have it.
    if (host_->keymap_path.empty()) {
        answer.file_refusal = "isolated run -- no keymap file; the change stands for this run";
    } else {
        std::error_code ec;
        const bool exists = std::filesystem::exists(host_->keymap_path, ec);
        bool changed = exists != keymap_file_present_;
        if (!changed && exists) {
            const persist::FileText now = persist::read_file(
                host_->keymap_path, keymap_persist::kMaxKeymapBytes, "a Workshop keymap");
            changed = !now.outcome.accepted || now.text != keymap_bytes_;
        }
        if (changed) {
            answer.file_refusal = "the keymap file changed since this Workshop read it -- not "
                                  "written; relaunch to read the file, then edit again";
        } else {
            const std::string text = keymap_persist::to_text(session_.keymap);
            const Written wrote = persist::write_file(host_->keymap_path, text);
            if (wrote.accepted) {
                answer.written = true;
                keymap_bytes_ = text;
                keymap_file_present_ = true;
            } else {
                answer.file_refusal = "not written: " + wrote.refusal;
            }
        }
    }
    if (answer.written) {
        session_.conditions.retract(kKeymapUnwrittenKey);
    } else {
        session_.conditions.establish(Condition{
            kKeymapUnwrittenKey, "a binding changed for this run only",
            "`" + asked.action + "` " + how + " -- " + answer.file_refusal, surface::role::kAlert,
            std::string()});
    }
    answer.sentence = "`" + asked.action + "` " + how +
                      (answer.written ? " -- written to the keymap file"
                                      : " -- " + answer.file_refusal);
    keymap_standing_ = answer.written
                           ? "edited -- " + std::to_string(session_.keymap.authored.size()) +
                                 " authored row" +
                                 (session_.keymap.authored.size() == 1 ? "" : "s") + " written"
                           : "edited for this run -- " + answer.file_refusal;
    say(answer.sentence, false);
    keymap_published_ = false;
    publish_keymap(mail);
    (void)mail.answer(answer);
    repaint(mail);
}

// WL-DESK-13 -- agents/workshop/desktop.md
void WorkshopWeave::on(const PaneToggleRequested& asked, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return; // an office, and only an office -- the seam's rule for changing the desk
    }
    const PaneRef ref{asked.office, asked.pane};
    PaneToggleAnswered answer;
    answer.office = asked.office;
    answer.pane = asked.pane;
    // JUDGED AGAINST THE DESK NOW, not against whatever the asker last heard: on the desk --
    // seated or waiting for room -- means hide; off it means show and focus. Through the two
    // doors a launch and a close already go through, so a toggle can do nothing they cannot.
    if (has_pane(session_.setup.active, ref)) {
        const PaneCloseAnswered closed = close_pane(ref, mail);
        answer.closed = closed.closed;
        answer.refusal = closed.refusal;
        if (closed.closed) {
            say("closed " + inventory_name(ref) +
                    " -- its provider and what it holds are untouched; launching it opens it again",
                false);
        }
    } else {
        const PaneLaunchAnswered opened = launch_pane(ref, mail);
        answer.opened = opened.opened;
        answer.refusal = opened.refusal;
        if (opened.opened) {
            say("opened " + inventory_name(ref), false);
        }
    }
    if (!answer.refusal.empty()) {
        say(answer.refusal, true);
    }
    (void)mail.answer(answer);
    repaint(mail);
}

// ---- Whether anybody is there to fill a pane -----------------------------------------------

// WL-DESK-04 -- agents/workshop/desktop.md
bool WorkshopWeave::provider_present(std::int64_t kind, const PaneRef& ref) const {
    if (!is_runtime_kind(kind)) {
        return kind != kNoPaneKind; // this host's own panes, and the maker's, are presented here
    }
    // THE OFFICE'S HOLDER AT THIS INSTANT, and whether it takes a room -- the bus's facts,
    // read and kept nowhere (`holder_accepts_on`). A host that wired no answer says no.
    return host_->holder_accepts &&
           host_->holder_accepts(ref.provider, *loom::schema_of<PaneRoom>());
}

// ---- The one inventory, said out loud ------------------------------------------------------

// WL-DESK-04 -- agents/workshop/desktop.md
PaneInventory WorkshopWeave::inventory_reading() const {
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
        // AVAILABLE IS ASKED NOW, of the office's current holder -- not read off the catalog,
        // which remembers every pane ever offered this run and so cannot say who left.
        p.available = row.kind != kNoPaneKind && provider_present(row.kind, row.ref);
        // ...AND WHETHER THE RUN STILL OWES IT, asked of the realization owner now. Absent and
        // still to come is not absent and settled: only the second is something to build.
        p.pending = !p.available && host_->office_pending && host_->office_pending(p.office);
        for (const std::int64_t k : seated.waiting) {
            if (k == row.kind) {
                p.waiting = true;
                break;
            }
        }
        said.panes.push_back(std::move(p));
    }
    return said;
}

// WL-DESK-09 -- agents/workshop/desktop-presenting.md
void WorkshopWeave::on(const PaneInventoryRequested&, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return; // an office asks; personal speech is answered by nobody, the offer's rule
    }
    // THE READING NOW, TO THE ONE WHO ASKED -- the incarnation that just arrived, and no other
    // (Loom ANS-03). The publication's record of its own last utterance is not touched: other
    // presenters have heard what they heard, and the next change is said to all of them.
    (void)mail.answer(inventory_reading());
}

// WL-DESK-04 -- agents/workshop/desktop.md
void WorkshopWeave::publish_inventory(loom::Mail& mail) {
    PaneInventory said = inventory_reading();
    // COMPARED BEFORE IT IS PUBLISHED, for `StandingConditions`' reason: a presenter told the same
    // thing twice repaints for nothing, and the inventory is derived at every gesture. A
    // presenter that has just arrived has heard nothing, so an offer clears the record.
    if (inventory_published_ && said.panes.size() == inventory_said_.size()) {
        bool same = true;
        for (std::size_t i = 0; i < said.panes.size(); ++i) {
            const InventoryPane& a = said.panes[i];
            const InventoryPane& b = inventory_said_[i];
            if (a.office != b.office || a.pane != b.pane || a.name != b.name ||
                a.summary != b.summary || a.open != b.open || a.available != b.available ||
                a.waiting != b.waiting || a.pending != b.pending) {
                same = false;
                break;
            }
        }
        if (same) {
            return;
        }
    }
    inventory_said_ = said.panes;
    inventory_published_ = true;
    // SAID AS THE OFFICE, never personally. A reader has to be able to tell this host's
    // reading from any weave's opinion, so the publication carries the office stamp and a
    // presenter refuses an unstamped one -- `say_conditions`' rule, one seam over.
    // `as_role` adds provenance, never a capability (MSG-07).
    (void)mail.as_role(kWorkshopProvider).publish(std::move(said));
}

// ---- The effective keymap, said out loud ---------------------------------------------------

namespace {

bool same_keymap(const KeymapShown& a, const KeymapShown& b) {
    if (a.file != b.file || a.word != b.word || a.rows.size() != b.rows.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.rows.size(); ++i) {
        const ShownBinding& x = a.rows[i];
        const ShownBinding& y = b.rows[i];
        if (x.group != y.group || x.id != y.id || x.label != y.label || x.gesture != y.gesture ||
            x.authored != y.authored || x.remappable != y.remappable) {
            return false;
        }
    }
    return true;
}

} // namespace

// WL-DESK-11 -- agents/workshop/desktop-presenting.md
void WorkshopWeave::publish_keymap(loom::Mail& mail) {
    KeymapShown said = keymap_shown(session_, host_->keymap_path, keymap_standing_);
    // COMPARED BEFORE IT IS PUBLISHED, `publish_inventory`'s rule: derived at every gesture,
    // said only when it moved, and said again after a presenter arrives.
    if (keymap_published_ && same_keymap(said, keymap_said_)) {
        return;
    }
    keymap_said_ = said;
    keymap_published_ = true;
    (void)mail.as_role(kWorkshopProvider).publish(std::move(said));
}

// WL-DESK-11 -- agents/workshop/desktop-presenting.md
void WorkshopWeave::on(const KeymapRequested&, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return; // an office asks
    }
    (void)mail.answer(keymap_shown(session_, host_->keymap_path, keymap_standing_));
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
