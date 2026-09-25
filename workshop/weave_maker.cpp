// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s maker-made pane: the one open definition, and the Pane Creator's three acts
// on it, asked by whichever office presents them.
// Workshop law: agents/workshop/maker-pane.md (+1 register; agents/workshop.md routes)

#include "weave.hpp"

namespace zengine::workshop {

// WL-MAKER-08 -- agents/workshop/maker-pane.md
std::string WorkshopWeave::pane_row_hotkey(const std::string& id, std::string* pane_name) const {
    // THE CREATOR'S KEYS ARE WHOEVER DECLARED THEM, AS THEY ARE IN FORCE: the effective keymap
    // holds a pane's rows while it has declared them, joined with the maker's own file, so the
    // spelling here is the one dispatch reads -- and a Workshop whose Pane Manager is not here
    // has no key to name, which the sentence then says in words.
    for (const PaneRows& declared : session_.keymap.panes) {
        for (const PaneRow& row : declared.rows) {
            if (row.id != id || !is_bound(row.gesture)) {
                continue;
            }
            if (pane_name != nullptr) {
                const RuntimePane* offered = session_.panels.runtime.of_kind(declared.pane);
                *pane_name = offered != nullptr ? offered->name : std::string("its pane");
            }
            return gesture_text(row.gesture);
        }
    }
    return std::string();
}

// WL-MAKER-08 -- agents/workshop/maker-pane.md
std::string WorkshopWeave::maker_pane_dirty_sentence(const char* consequence) const {
    const MakerPane& m = session_.panels.maker;
    const std::string name = m.definition.open() ? m.definition.name : m.saved.name;
    std::string where;
    const std::string save = pane_row_hotkey(kCreatorSaveId, &where);
    const std::string discard = pane_row_hotkey(kCreatorDiscardId, nullptr);
    // ⚠ NO KEY NAMED IS NOT "NO PANE MANAGER". The presenter declares its rows as its mode calls
    // for, and while its own name line is open it declares that line's two and no more -- which
    // is exactly when a second pane is asked for over a dirty one. So the sentence without keys
    // names the owner and claims nothing about whether it is here.
    const std::string how = !save.empty() && !discard.empty()
                                ? save + " in " + where + " saves it, " + discard +
                                      " discards them"
                                : std::string("the Pane Manager saves or discards them");
    return "pane " + name + " has unsaved changes -- " + how + "; " + consequence;
}

// WL-MAKER-08 -- agents/workshop/maker-pane.md
void WorkshopWeave::open_maker_pane(const std::string& requested, loom::Mail& mail) {
    const std::string path = persist::resolved_against(host_->project_dir, requested);
    MakerPane& m = session_.panels.maker;
    if (m.dirty()) {
        say(maker_pane_dirty_sentence("nothing was opened"), true);
        return;
    }
    const pane_definition_persist::LoadedDefinition loaded =
        pane_definition_persist::load_file(path);
    if (!loaded.outcome.accepted) {
        if (path == host_pane_path()) {
            pane_refused_ = true;
            session_.conditions.establish(Condition{
                kPaneWallKey, "pane definition refused -- this run will not write over it",
                loaded.outcome.refusal +
                    " (the file is left exactly as it is; fix it, or start Workshop with "
                    "--pane <another path>)",
                surface::role::kAlert, std::string()});
        }
        say(path + ": " + loaded.outcome.refusal, true);
        return;
    }
    m.path = path;
    m.definition = loaded.definition;
    m.saved = loaded.definition;
    // AND THE DESK RE-SEATS, because a reference that was unresolved a moment ago may
    // resolve now -- the same reconcile a provider's late offer earns.
    apply_setup(mail);
    say("opened pane " + m.definition.name + " from " + path, false);
}

// WL-MAKER-08, WL-MAKER-11 -- agents/workshop/maker-pane.md
bool WorkshopWeave::new_maker_pane(const std::string& name, loom::Mail& mail) {
    MakerPane& m = session_.panels.maker;
    if (m.dirty()) {
        say(maker_pane_dirty_sentence("nothing was made"), true);
        return false;
    }
    // THE NAME'S OWN REFUSAL, AND NOTHING ABOUT KEYS: the asker holds the line the name was
    // typed into and says how to try again in its own words.
    const Written legal = check_maker_pane_name(name);
    if (!legal.accepted) {
        say(legal.refusal, true);
        return false;
    }
    const PaneRef ref = maker_pane_ref(name);
    m.definition = new_definition(name);
    m.saved = PaneDefinition{};
    m.path = host_pane_path();
    (void)add_pane(session_.setup.active, ref);
    const Seating trial = seat_panes(session_.setup.active, session_.panels,
                                     stack_capacity(screen_of(session_)));
    bool waiting = false;
    for (const std::int64_t k : trial.waiting) {
        if (is_maker_kind(k)) {
            waiting = true;
        }
    }
    apply_setup(mail);
    // ⚠ AND NOTHING HERE CHOOSES WHAT IS INSPECTED. The host's Pane Manager made the new pane its
    // subject and put the keys on its Text row; an inspector's subject is the inspector's to name
    // (WL-INFO-14), so the sentence says where the region's rows are instead.
    std::string where;
    const std::string save = pane_row_hotkey(kCreatorSaveId, &where);
    std::string said = "Pane Creator: " + name + " is on this layout";
    if (waiting) {
        said += " (waiting for room -- make the window taller)";
    }
    said += " -- inspect it in Info to write its text region";
    if (!save.empty()) {
        said += "; " + save + " in " + where + " saves it";
    }
    if (m.path.empty()) {
        said += " (no pane file this run)";
    }
    say(said, false);
    return true;
}


// WL-MAKER-08 -- agents/workshop/maker-pane.md
bool WorkshopWeave::save_maker_pane() {
    MakerPane& m = session_.panels.maker;
    if (!m.open()) {
        std::string where;
        const std::string make = pane_row_hotkey(kCreatorNewId, &where);
        say("no pane is open -- " +
                (make.empty() ? std::string("the Pane Manager makes one")
                              : make + " in " + where + " makes one"),
            true);
        return false;
    }
    if (m.path.empty()) {
        say(kNoPaneFile, true);
        return false;
    }
    if (pane_refused_ && m.path == host_pane_path()) {
        say("the pane file " + m.path +
                " was refused at startup and will not be written over -- fix it, or start "
                "Workshop with --pane <another path>",
            true);
        return false;
    }
    // A half-typed value is not seen here: the draft a region's text is typed into is Info's, and
    // this door writes the definition as its owner holds it.
    const Written written = pane_definition_persist::save_file(m.path, m.definition);
    if (!written.accepted) {
        say(written.refusal, true);
        return false;
    }
    m.saved = m.definition;
    say("saved pane " + m.definition.name + " to " + m.path, false);
    return true;
}

// WL-MAKER-08 -- agents/workshop/maker-pane.md
bool WorkshopWeave::discard_maker_pane_edits(loom::Mail& mail) {
    MakerPane& m = session_.panels.maker;
    if (!m.open() && !m.saved.open()) {
        say("no pane is open -- nothing to discard", true);
        return false;
    }
    if (!m.dirty()) {
        say("pane " + m.definition.name + " matches " + m.path + " -- nothing to discard",
            false);
        return true;
    }
    const std::string was = m.definition.name;
    m.definition = m.saved;
    apply_setup(mail);
    if (m.open()) {
        say("discarded unsaved edits -- pane " + m.definition.name + " is back to what " +
                m.path + " holds",
            false);
    } else {
        say("discarded pane " + was +
                " -- it was never saved, so no pane is open; its row on this layout is "
                "kept and reads unresolved",
            false);
    }
    return true;
}

// WL-MAKER-11 -- agents/workshop/maker-pane.md
void WorkshopWeave::on(const MakerPaneRequested& asked, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return; // an office asks; personal speech is answered by nobody
    }
    MakerPaneAnswered answer;
    answer.act = asked.act;
    switch (asked.act) {
    case maker_pane_act::kCreate: answer.accepted = new_maker_pane(asked.name, mail); break;
    case maker_pane_act::kSave: answer.accepted = save_maker_pane(); break;
    case maker_pane_act::kDiscard: answer.accepted = discard_maker_pane_edits(mail); break;
    default:
        say("no such act on the maker's pane -- make, save and discard are the three", true);
        break;
    }
    // THE ASKER IS TOLD WHAT THE BAND SAYS, in the same words: one sentence, two readers, and
    // Loom delivers this answer only to the incarnation that asked (ANS-03).
    answer.said = session_.notice;
    (void)mail.answer(answer);
    repaint(mail);
}

} // namespace zengine::workshop
