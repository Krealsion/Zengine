// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s inspection seam: a pane an inspector names, the picture of its rows, and one
// write through the rows' owner, said with how far a setting's write got.
// Workshop law: agents/workshop/info-body.md (+1 register; agents/workshop.md routes)

#include "weave.hpp"

#include <memory>
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
    const PaneSettingsShown read = store().read_pane(std::string(), PaneSettingsRequested{
                                                                     in.ref.provider, in.ref.pane});
    std::vector<std::string> locked;
    std::vector<std::string> locked_here;
    for (const SettingLockShown& l : read.locks) {
        locked.push_back(l.key);
        if (l.scope == kLockScopeLayout) {
            locked_here.push_back(l.key);
        }
    }
    const SettingsSubject settings = settings_subject(session_, in.ref, store_.counted_v1(in.ref),
                                                      std::move(locked), std::move(locked_here));
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
        (!host_->holder_accepts(ref.provider, *loom::schema_of<v2::PaneSettings>()) &&
         !host_->holder_accepts(ref.provider, *loom::schema_of<PaneSettings>()))) {
        return "stored; not handed: " + ref.provider + " takes no settings";
    }
    const SetupPane* seat = pane_of(session_.setup.active, ref);
    // HANDED AT THE ROW'S REVISION, when the holder is handed one: settings equal to an earlier
    // revision's are not what this row was last minted for.
    const bool current =
        pane->settings_revision == 0 || pane->settings_revision == settings_revision(ref);
    if (seat == nullptr || !pane->settings_heard ||
        *pane->settings_heard != pane_held_settings(seat->settings) || !current) {
        return "stored; not handed: the hand-off was not queued";
    }
    // ...AND WHAT THE PANE SAID IT CANNOT USE OF WHAT IT WAS HANDED, in its own words.
    std::string said = "stored and handed";
    for (const ExternalPane::UnusedSetting& u : pane->settings_unused) {
        said += "; it cannot use " + u.key + " -- " + u.why;
    }
    return said;
}

// WL-SETTING-09 -- agents/workshop/settings.md
std::string WorkshopWeave::settings_asker_refused(const loom::Mail& mail) const {
    if (!mail.authored_role().empty()) {
        return std::string(); // an office: code loaded into this process, or a host's own weave
    }
    if (host_->terminal != nullptr && mail.sender() == host_->terminal->id()) {
        return std::string(); // the Terminal this host mounted, under its own widening
    }
    const scope::GuestRowFacts guest =
        host_->guest_row ? host_->guest_row(mail.sender()) : scope::GuestRowFacts{};
    if (guest.admitted) {
        if (guest.may(scope::kPowerSettings)) {
            return std::string();
        }
        return "guest '" + guest.name + "' has no `" + scope::kPowerSettings +
               "` in its row -- only `" + scope::kPowerSettings + "` reads and writes a pane's "
               "settings";
    }
    return "a pane's settings are read and written by an office, this Workshop's Terminal or a "
           "guest with `" + std::string(scope::kPowerSettings) + "`";
}

// WL-SETTING-07 -- agents/workshop/settings.md
void WorkshopWeave::on(const PaneSettingsRequested& asked, loom::Mail& mail) {
    const std::string refused = settings_asker_refused(mail);
    PaneSettingsShown shown = store().read_pane(refused, asked);
    take_store(mail);
    if (refused.empty() && shown.said.empty()) {
        // WHAT THE STORE KEEPS, and how far its hand-off got, which is this desk's to say.
        shown.said = settings_handed(PaneRef{asked.provider, asked.pane});
    }
    (void)mail.answer(std::move(shown));
}

// WL-SETTING-07 -- agents/workshop/settings.md
void WorkshopWeave::on(const PaneSettingCommitRequested& asked, loom::Mail& mail) {
    PaneSettingActed acted = store().commit_pane(settings_asker_refused(mail), asked);
    const StoreEffects fx = store_.take_effects();
    if (fx.live_moved) {
        install_live(store_.live_layout(), mail);
    }
    hand_office_settings(mail);
    if (acted.accepted) {
        // A SETTING'S WRITE IS HANDED NOW, at the revision it minted, and said with how far it got.
        acted.handed = settings_handed(PaneRef{asked.provider, asked.pane});
        if (!fx.said.empty()) {
            say(fx.said + "; " + acted.handed, false);
        }
    }
    (void)mail.answer(std::move(acted));
    repaint(mail);
}

// WL-SETTING-08 -- agents/workshop/settings.md
void WorkshopWeave::on(const PaneSettingUnused& unused, loom::Mail& mail) {
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // personal speech says nothing about a pane (the offer's rule)
    }
    const RuntimePane* row = session_.panes.runtime.find(office, unused.pane);
    ExternalPane* pane = row != nullptr ? session_.panes.external_pane(row->kind) : nullptr;
    if (row == nullptr || pane == nullptr) {
        return; // a pane this office never offered, or one not seated
    }
    // ABOUT WHAT IT HOLDS NOW, or about nothing: a word naming a revision it was not last handed
    // is late, and is dropped as the revision it names was.
    const PaneRef ref{row->provider, row->pane};
    const std::int64_t current = settings_revision(ref);
    if (unused.revision <= 0 || unused.revision != current ||
        pane->settings_revision != current) {
        return;
    }
    const std::string at = row->name + " @" + row->provider + ": ";
    std::string wrong = pane_setting_key_problem(unused.key);
    if (wrong.empty() && (unused.why.empty() || !pane_setting_text_ok(unused.why))) {
        wrong = "why a setting is unused is 1 to " + std::to_string(kMaxPaneSettingTextLen) +
                " bytes of printable ASCII";
    }
    if (!wrong.empty()) {
        say(at + "a setting it cannot use was not kept -- " + wrong, true);
        repaint(mail);
        return;
    }
    for (ExternalPane::UnusedSetting& kept : pane->settings_unused) {
        if (kept.key == unused.key) {
            if (kept.why == unused.why) {
                return; // said again: nothing new to tell
            }
            kept.why = unused.why;
            repaint(mail);
            return;
        }
    }
    if (pane->settings_unused.size() >= kMaxPaneSettingsPerRow) {
        say(at + "a setting it cannot use was not kept -- a pane keeps at most " +
                std::to_string(kMaxPaneSettingsPerRow) + " of them",
            true);
        repaint(mail);
        return;
    }
    pane->settings_unused.push_back(
        ExternalPane::UnusedSetting{unused.key, unused.revision, unused.why});
    repaint(mail);
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
    session_.pending_edit.reset();
    session_.pending_setting.reset();
    const Commit result = row.commit_text(asked.text);
    if (result != Commit::Accepted) {
        // THE OWNER'S OWN WORDS: an amount the face does not read, a pane that is not in this
        // desk, a definition that is not open -- each worded by its setter.
        answer(false, row.label() + ": " + row.refusal());
        return;
    }
    const std::string label = row.label();
    const std::string name = pane_subject_shown(session_).name;
    const PaneRef ref = in.ref;
    // WHAT THE ROW'S SETTER ASKED, asked of the store: the row's window, or one of its settings.
    if (session_.pending_setting) {
        PaneSettingCommitRequested commit = *session_.pending_setting;
        session_.pending_setting.reset();
        // JUDGED NOW: the subject's own number already said the weaver saw this row as it stands.
        commit.revision = settings_revision(ref);
        const PaneSettingActed acted = store().commit_pane(std::string(), commit);
        (void)store_.take_effects().said; // the sentence is this door's, below
        install_live(store_.live_layout(), mail);
        hand_office_settings(mail);
        if (!acted.accepted) {
            answer(false, label + ": " + acted.refusal);
            repaint(mail);
            return;
        }
        // A SETTING'S WRITE IS HANDED NOW, before the room this repaint may grant, and the
        // sentence says how far it got: stored is not handed, and handed is not used.
        refresh_inspected();
        const std::string value = row_value_of(label);
        say("committed " + label + " of " + name + " = " + value + "; " + settings_handed(ref),
            false);
        answer(true, std::string());
        repaint(mail);
        return;
    }
    if (!session_.pending_edit) {
        answer(true, std::string());
        return;
    }
    const LayoutEdit edit = *session_.pending_edit;
    session_.pending_edit.reset();
    // A PLACEMENT WRITE IS RECONCILED as the hand's is: the live layout the store sends back is
    // presented, which opens and closes what it seats.
    loom::DeferredAnswer owed = mail.defer_answer();
    auto held = std::make_shared<loom::DeferredAnswer>(std::move(owed));
    ask_layout({edit},
               [this, held, label, name](const LayoutEditAnswered& answered, loom::Mail& m) {
                   if (!answered.accepted) {
                       (void)loom::answer_deferred(*held, m,
                                                   PaneSubjectActed{false, label + ": " +
                                                                               answered.refusal});
                       repaint(m);
                       return;
                   }
                   refresh_inspected();
                   say("committed " + label + " of " + name + " = " + row_value_of(label), false);
                   (void)loom::answer_deferred(*held, m, PaneSubjectActed{true, std::string()});
                   repaint(m);
               },
               mail);
}

// The value the subject's row `label` shows now, or empty.
std::string WorkshopWeave::row_value_of(const std::string& label) const {
    for (const Row& row : session_.inspected.rows) {
        if (row.label() == label) {
            return row.value();
        }
    }
    // A ROW THE WRITE TOOK AWAY -- a kept key cleared, which nothing declares -- holds nothing now.
    return "--";
}

} // namespace zengine::workshop
