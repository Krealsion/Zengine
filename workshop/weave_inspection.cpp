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
    const SettingsSubject settings =
        settings_subject(session_, in.ref,
                         offered != nullptr ? counted_settings(*offered) : nullptr,
                         locked_keys(in.ref), layout_locked_keys(in.ref));
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
    bool current = pane->settings_revision == 0;
    for (const SettingsRevision& r : settings_revisions_) {
        current = current || (r.ref == ref && r.revision == pane->settings_revision);
    }
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
    PaneSettingsShown shown;
    shown.provider = asked.provider;
    shown.pane = asked.pane;
    if (const std::string refused = settings_asker_refused(mail); !refused.empty()) {
        shown.said = "settings withheld: " + refused;
        (void)mail.answer(std::move(shown));
        return;
    }
    const PaneRef ref{asked.provider, asked.pane};
    const RuntimePane* offered = session_.panes.runtime.find(ref.provider, ref.pane);
    load_prefs();
    if (const std::vector<v2::PaneSettingRow>* rows =
            offered != nullptr ? counted_settings_v2(*offered) : nullptr) {
        shown.rows = *rows;
    }
    shown.pins = effective_pins(ref);
    shown.locks = locks_on(ref, session_.setup.active_at);
    const SetupPane* seat = pane_of(session_.setup.active, ref);
    if (seat == nullptr) {
        shown.said = ref_text(ref) + " has no row on the live layout";
        (void)mail.answer(std::move(shown));
        return;
    }
    shown.revision = settings_revision(ref);
    shown.settings = seat->settings;
    shown.said = settings_handed(ref);
    (void)mail.answer(std::move(shown));
}

// WL-SETTING-07 -- agents/workshop/settings.md
void WorkshopWeave::on(const PaneSettingCommitRequested& asked, loom::Mail& mail) {
    const auto refuse = [&mail](std::string why, std::int64_t revision) {
        (void)mail.answer(PaneSettingActed{false, std::move(why), revision, std::string()});
    };
    // IN ORDER, EACH REFUSAL WRITING NOTHING: the asker, the row, the revision, then the value.
    if (const std::string refused = settings_asker_refused(mail); !refused.empty()) {
        refuse("settings commit withheld: " + refused, 0);
        return;
    }
    const PaneRef ref{asked.provider, asked.pane};
    if (pane_of(session_.setup.active, ref) == nullptr) {
        refuse(ref_text(ref) + " has no row on the live layout -- open it first", 0);
        return;
    }
    const std::int64_t current = settings_revision(ref);
    if (asked.revision != current) {
        refuse("stale revision " + std::to_string(asked.revision) + " -- the row of " +
                   ref_text(ref) + " stands at " + std::to_string(current) + "; read it again",
               current);
        return;
    }
    const RuntimePane* offered = session_.panes.runtime.find(ref.provider, ref.pane);
    const std::string name = offered != nullptr ? offered->name : ref_text(ref);
    const std::vector<PaneSetting> before = pane_of(session_.setup.active, ref)->settings;
    const int fields = (asked.flag ? 1 : 0) + (asked.number ? 1 : 0) + (asked.text ? 1 : 0);
    // A LOCK HOLDS ITS KEY against every write, a clear included.
    load_prefs();
    for (const std::string& key : locked_keys(ref)) {
        if (key == asked.key) {
            refuse(locked_setting_refusal(asked.key), current);
            return;
        }
    }
    std::string written;
    if (asked.clear) {
        // A CLEAR NAMES A KEY ALONE, declared or kept, and clearing an absent key changes nothing.
        if (fields != 0) {
            refuse("a clear takes no value -- name the key alone", current);
            return;
        }
        if (find_pane_setting(before, asked.key) != nullptr) {
            const Written w = author_pane_setting(session_.setup.active, ref, asked.key,
                                                  std::nullopt, std::nullopt);
            if (!w.accepted) {
                refuse(w.refusal, current);
                return;
            }
        }
        written = "cleared " + asked.key + " of " + name;
    } else {
        const std::vector<PaneSettingRow>* rows =
            offered != nullptr ? counted_settings(*offered) : nullptr;
        const PaneSettingRow* declared = nullptr;
        for (std::size_t i = 0; rows != nullptr && i < rows->size(); ++i) {
            declared = (*rows)[i].key == asked.key ? &(*rows)[i] : declared;
        }
        // WORKSHOP'S OWN WORD FOR THE PANE'S AREA, whatever the pane declares.
        const PaneSettingRow skin_row = narrow_setting_row(workshop_skin_setting_row());
        if (asked.key == kWorkshopSkinKey) {
            declared = &skin_row;
        }
        if (declared == nullptr) {
            refuse(ref_text(ref) + " has no counted setting `" + asked.key +
                       "` -- a key no declaration takes is only cleared",
                   current);
            return;
        }
        const PaneSetting candidate{asked.key, asked.flag, asked.number, asked.text};
        if (const std::string wrong = pane_setting_refused_by(*declared, candidate);
            !wrong.empty()) {
            refuse(wrong, current);
            return;
        }
        const Written w = author_pane_setting(session_.setup.active, ref, asked.key, candidate,
                                              row_setting_absence(*declared));
        if (!w.accepted) {
            refuse(w.refusal, current);
            return;
        }
        written = "committed " + asked.key + " of " + name + " = " + setting_value_text(candidate);
    }
    if (pane_of(session_.setup.active, ref)->settings == before) {
        // NOTHING MOVED, so nothing is minted or handed: the asker's revision still stands.
        (void)mail.answer(PaneSettingActed{true, std::string(), current, settings_handed(ref)});
        return;
    }
    // A SETTING'S WRITE IS HANDED NOW, at the revision it mints, and said with how far it got.
    hand_settings(mail);
    const std::string handed = settings_handed(ref);
    say(written + "; " + handed, false);
    (void)mail.answer(PaneSettingActed{true, std::string(), settings_revision(ref), handed});
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
