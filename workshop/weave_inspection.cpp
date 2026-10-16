// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s inspection seam: a pane an inspector names, the picture of its rows, and one
// write through the rows' owner, said with how far a setting's write got.
// Workshop law: agents/workshop/info-body.md (+1 register; agents/workshop.md routes)

#include "weave.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

// WL-INFO-14 -- agents/workshop/info-body.md
void WorkshopWeave::refresh_inspected() {
    InspectedPane& in = session_.inspected;
    if (!in.addressed()) {
        return;
    }
    // THE FOUR THINGS A NAME STANDS FOR: this pane (the door keeps it), this desk, whether it holds
    // the pane's row, and the settings the rows were built from. A value moving is none of them --
    // the rows read fresh -- a row leaving or rejoining the desk is one, so a draft never lands on
    // a row made afresh, and a provider arriving or leaving is one only as its declaration starts
    // or stops counting.
    const RuntimePane* offered = session_.panes.runtime.find(in.ref.provider, in.ref.pane);
    const SettingsSubject settings = settings_subject(
        session_, in.ref, offered != nullptr ? counted_settings(*offered) : nullptr);
    const bool member = has_pane(session_.setup.active, in.ref);
    if (in.name != 0 && in.desk == session_.setup.put_live && in.member == member &&
        in.settings == settings) {
        return;
    }
    in.rows = pane_subject_rows(session_, in.ref, settings);
    in.desk = session_.setup.put_live;
    in.member = member;
    in.settings = settings;
    in.name = ++in.minted;
}

// WL-INFO-14 -- agents/workshop/info-body.md
void WorkshopWeave::on(const InspectPaneRequested& asked, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return; // an office asks; personal speech is answered by nobody
    }
    const PaneRef ref{asked.office, asked.pane};
    bool known = false;
    for (const CatalogRow& row : inventory_rows(session_.setup.active, session_.panes)) {
        if (row.ref == ref) {
            known = true;
            break;
        }
    }
    if (ref.provider.empty() || !known) {
        (void)mail.answer(PaneSubjectActed{
            false, ref_text(ref) +
                       " is in neither this build's vocabulary nor this desk -- nothing to inspect"});
        return;
    }
    InspectedPane& in = session_.inspected;
    if (!(in.ref == ref)) {
        in.ref = ref;
        in.name = 0; // another pane: named afresh below, so no draft typed for the last one lands
    }
    refresh_inspected();
    (void)mail.answer(PaneSubjectActed{true, std::string()});
    repaint(mail);
}

// WL-INFO-14 -- agents/workshop/info-body.md
void WorkshopWeave::on(const PaneSubjectRequested&, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return;
    }
    refresh_inspected();
    (void)mail.answer(pane_subject_shown(session_));
}

// WL-INFO-14 -- agents/workshop/info-body.md
void WorkshopWeave::publish_pane_subject(loom::Mail& mail) {
    // NOTHING IS SAID ABOUT A SUBJECT NOBODY NAMED. An inspector that arrives asks and is answered
    // "none" (`on(PaneSubjectRequested)`); until one names a pane, this host has no sentence here.
    if (!session_.inspected.addressed()) {
        return;
    }
    refresh_inspected();
    PaneSubjectShown said = pane_subject_shown(session_);
    if (subject_published_ && same_pane_subject(said, subject_said_)) {
        return; // no news is silence, and silence is what makes this seam terminate
    }
    subject_said_ = said;
    subject_published_ = true;
    (void)mail.as_role(kWorkshopProvider).publish(std::move(said));
}

// WL-SETTING-05 -- agents/workshop/settings.md
std::string WorkshopWeave::settings_handed(const PaneRef& ref) const {
    const std::optional<std::int64_t> kind = resolve_pane(ref, session_.panes);
    if (!kind.has_value()) {
        return "stored; not handed: nothing offers " + ref_text(ref) + " here";
    }
    if (!is_runtime_kind(*kind)) {
        return "stored; not handed: Workshop's own pane takes no settings";
    }
    const ExternalPane* pane = session_.panes.external_pane(*kind);
    if (pane == nullptr || !session_.panes.has(*kind)) {
        return "stored; not handed: the pane is not seated";
    }
    if (!host_->role_holder || !host_->role_holder(ref.provider).valid()) {
        return "stored; not handed: nothing holds `" + ref.provider + "` now";
    }
    if (!host_->holder_accepts ||
        !host_->holder_accepts(ref.provider, *loom::schema_of<PaneSettings>())) {
        return "stored; not handed: " + ref.provider + " takes no settings";
    }
    const SetupPane* seat = pane_of(session_.setup.active, ref);
    if (seat != nullptr && pane->settings_heard && *pane->settings_heard == seat->settings) {
        return "stored and handed";
    }
    return "stored; not handed: the hand-off was not queued";
}

// WL-INFO-15 -- agents/workshop/info-body.md
void WorkshopWeave::on(const PaneCommitRequested& asked, loom::Mail& mail) {
    if (mail.authored_role().empty()) {
        return;
    }
    const auto answer = [&mail](bool accepted, std::string refusal) {
        (void)mail.answer(PaneSubjectActed{accepted, std::move(refusal)});
    };
    // THE NAME FIRST, BEFORE ANY ROW IS READ OR ANY SETTER REACHED -- and judged against the
    // facts as they are now, so a desk put live since the picture was drawn is caught here
    // even though no repaint has named it yet.
    refresh_inspected();
    InspectedPane& in = session_.inspected;
    if (in.name == 0 || asked.subject != in.name) {
        answer(false, kPaneCommitSubjectGone);
        return;
    }
    if (asked.row < 0 || static_cast<std::size_t>(asked.row) >= in.rows.size()) {
        answer(false, "that row is not in this pane's properties any more");
        return;
    }
    Row& row = in.rows[static_cast<std::size_t>(asked.row)];
    if (!row.editable()) {
        answer(false, row.label() + " is not authored -- it is what the screen makes of the "
                                    "authored value");
        return;
    }
    const SetupPane* before_row = pane_of(session_.setup.active, in.ref);
    const std::vector<PaneSetting> before =
        before_row != nullptr ? before_row->settings : std::vector<PaneSetting>{};
    const Commit result = row.commit_text(asked.text);
    if (result != Commit::Accepted) {
        // THE OWNER'S OWN WORDS: an amount the face does not read, a pane that is not in this
        // desk, a definition that is not open -- each worded by its setter.
        answer(false, row.label() + ": " + row.refusal());
        return;
    }
    // SAID WITH WHAT WAS WRITTEN, TO WHICH PANE, read before the reseat below touches anything.
    std::string written = "committed " + row.label() + " of " +
                          pane_subject_shown(session_).name + " = " + row.value();
    const SetupPane* after_row = pane_of(session_.setup.active, in.ref);
    const bool settings_moved = after_row != nullptr && after_row->settings != before;
    // A PLACEMENT WRITE IS RECONCILED as the hand's is (`arrange_place`): an authored place
    // takes the pane out of the stack, and `apply_setup` is the one door that opens or closes a
    // pane -- for a place, with no room rationing the stack, it opens and closes nothing.
    apply_setup(mail);
    if (settings_moved) {
        // A SETTING'S WRITE IS HANDED NOW, before the room this repaint may grant, and the
        // sentence says how far it got: stored is not handed, and handed is not used.
        hand_settings(mail);
        written += "; " + settings_handed(in.ref);
    }
    say(written, false);
    answer(true, std::string());
    repaint(mail);
}

} // namespace zengine::workshop
