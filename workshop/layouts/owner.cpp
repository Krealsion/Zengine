// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The layouts' owner: each door asked of the store, judged for its asker, answered to that asker;
// and what the store moved sent after it -- each pane's row, the presenter's picture, each
// office's settings.
// Workshop law: agents/workshop/layouts.md, agents/workshop/settings.md,
// agents/workshop/setting-locks.md, agents/workshop/desktop.md

#include "workshop/layouts/owner.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace zengine::workshop {

// ---- Who asks -----------------------------------------------------------------------------------

void LayoutsOwner::read_host() {
    if (host_.store) {
        readings_ = host_.store();
    }
}

bool LayoutsOwner::from_presenter(const loom::Mail& mail) const {
    return mail.authored_from_role(kWorkshopProvider);
}

bool LayoutsOwner::from_terminal(const loom::Mail& mail) const {
    if (!host_.terminal || !mail.authored_role().empty()) {
        return false;
    }
    const loom::WeaveId terminal = host_.terminal();
    return terminal.valid() && mail.sender() == terminal;
}

// WL-SETTING-09 -- agents/workshop/settings.md
std::string LayoutsOwner::asker_refused(const loom::Mail& mail) const {
    if (!mail.authored_role().empty()) {
        return std::string(); // an office: code loaded into this process, or a host's own weave
    }
    if (from_terminal(mail)) {
        return std::string(); // the Terminal this host mounted, under its own widening
    }
    const scope::GuestRowFacts guest =
        host_.guest_row ? host_.guest_row(mail.sender()) : scope::GuestRowFacts{};
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

// ---- What moved, sent ---------------------------------------------------------------------------

// WL-LAYOUT-01 -- agents/workshop/layouts.md
void LayoutsOwner::send_live(loom::Mail& mail, const StoreEffects& fx) {
    LiveLayout live = store_.live_layout();
    live.said = fx.said;
    live.bad = fx.bad;
    // THE RESTORE'S OWN SENTENCE, ONCE: the presenter says it with the room it opened in.
    if (!restore_said_ && (restored_.present || !restored_.folded.empty())) {
        restore_said_ = true;
        if (restored_.present) {
            live.restored = true;
            live.said = restored_.head;
            live.bad = false;
            for (const std::string& part : {restored_.converted, restored_.folded}) {
                if (!part.empty()) {
                    live.restore_tail += "; " + part;
                }
            }
        } else {
            live.said = restored_.folded;
            live.bad = false;
        }
    }
    if (mail.as_role(kLayoutsRole).send_to_role(kWorkshopProvider, std::move(live)).valid()) {
        ++state_.pictures;
    }
}

// WL-SETTING-04 -- agents/workshop/settings.md
void LayoutsOwner::hand_rows(loom::Mail& mail) {
    for (const PaneHandOff& owed : store_.pane_hand_offs()) {
        // TO THE HOLDER ITSELF, so what is recorded is who heard it; Workshop's own keys stay here.
        const loom::Ticket sent =
            owed.revisions
                ? mail.as_role(kLayoutsRole).send(owed.holder, owed.value)
                : mail.as_role(kLayoutsRole)
                      .send(owed.holder, PaneSettings{owed.value.pane, owed.value.settings});
        if (sent.valid()) {
            store_.pane_handed(owed); // nothing recorded otherwise, so the next settle hands again
        }
    }
}

void LayoutsOwner::settle(loom::Mail& mail, bool picture) {
    // EACH PANE'S ROW FIRST, so a written setting's sentence says how far its hand-off got.
    hand_rows(mail);
    StoreEffects fx = store_.take_effects();
    if (!fx.said_provider.empty()) {
        fx.said += "; " + store_.handed_fact(PaneRef{fx.said_provider, fx.said_pane});
    }
    if (fx.live_moved || picture || !fx.said.empty() || fx.unused_moved) {
        send_live(mail, fx);
    }
    // EACH DECLARING OFFICE'S HOLDER, WORKSHOP'S AMONG THEM, IS HANDED ITS SETTINGS AT EACH
    // REVISION, when it takes them.
    for (auto& [office, settings] : store_.office_hand_offs()) {
        const std::int64_t revision = settings.revision;
        if (mail.as_role(kLayoutsRole).send_to_role(office, std::move(settings)).valid()) {
            store_.office_handed(office, revision);
        }
    }
}

// ---- The presenter's link -----------------------------------------------------------------------

// WL-LAYOUT-01 -- agents/workshop/layouts.md
// WL-SESSION-14 -- agents/workshop/session-restore.md
void LayoutsOwner::on(const LayoutEditRequested& asked, loom::Mail& mail) {
    read_host();
    if (!from_presenter(mail)) {
        // THE PRESENTER'S GESTURES, judged for their hand already: no door for anyone else.
        ++state_.refused;
        (void)mail.answer(LayoutEditAnswered{
            false,
            "a layout is edited by its presenter, " + std::string(kWorkshopProvider) +
                " -- the settings doors are every other asker's",
            store_.revision(),
            {},
            {}});
        return;
    }
    ++state_.edits;
    // THE START: the presenter's first ask restores the last session, once per process, and asks
    // which panes it presents.
    if (!started_) {
        started_ = true;
        restored_ = store_.restore();
        (void)mail.as_role(kLayoutsRole).send_to_role(kWorkshopProvider, PaneInventoryRequested{});
    }
    LayoutEditAnswered out;
    if (asked.edits.empty()) {
        out.accepted = true;
    } else {
        out = store_.edit(asked);
    }
    // THE PICTURE GOES FIRST, so the answer names the revision the presenter holds by then.
    settle(mail, asked.edits.empty());
    out.revision = store_.revision();
    (void)mail.answer(std::move(out));
}

// WL-SESSION-13 -- agents/workshop/session.md
void LayoutsOwner::on(const SessionSaveRequested&, loom::Mail& mail) {
    read_host();
    if (!from_presenter(mail)) {
        ++state_.refused;
        (void)mail.answer(SessionSaveAnswered{false, "the session is written at the presenter's "
                                                     "orderly close, and asked by it alone"});
        return;
    }
    const std::string refused = store_.save_session();
    if (refused.empty()) {
        ++state_.saves;
    }
    settle(mail);
    (void)mail.answer(SessionSaveAnswered{refused.empty(), refused});
}

// WL-PANE-10 -- agents/workshop/panes-and-windows.md
void LayoutsOwner::on(const PaneInventory& said, loom::Mail& mail) {
    read_host();
    // THE PRESENTER'S PUBLICATION, or its answer to this office's ask, which Loom verifies.
    if (!from_presenter(mail) && !mail.answers_ask()) {
        return; // a presenter's word about what it presents, and nobody else's
    }
    std::vector<CatalogName> names;
    names.reserve(said.panes.size());
    for (const InventoryPane& p : said.panes) {
        names.push_back(CatalogName{PaneRef{p.office, p.pane}, p.name, p.available});
    }
    store_.set_catalog(std::move(names));
    // A PANE NOW OFFERED IS HANDED ITS ROW.
    settle(mail);
}

// WL-LOCK-02 -- agents/workshop/setting-locks.md
void LayoutsOwner::on(const WeaversActAnswered& answered, loom::Mail& mail) {
    read_host();
    // AN ANSWER IS LOOM'S TO VERIFY, to the question this office sent the presenter.
    if (!mail.answers_ask()) {
        return;
    }
    const auto at = std::find_if(unlocks_.begin(), unlocks_.end(), [&](const PendingUnlock& p) {
        return p.question == mail.correlation();
    });
    if (at == unlocks_.end()) {
        return; // an answer to a question this owner no longer holds
    }
    PendingUnlock pending = std::move(*at);
    unlocks_.erase(at);
    PaneSettingLockAnswered acted = store_.lock(pending.refused, pending.asked, answered.current);
    settle(mail);
    (void)loom::answer_deferred(pending.answer, mail, std::move(acted));
}

// ---- A desk put live from a caller's text -------------------------------------------------------

// WL-LAYOUT-05 -- agents/workshop/layouts.md
void LayoutsOwner::on(const SetupApplyRequested& asked, loom::Mail& mail) {
    read_host();
    const std::string refused = store_.apply_setup(asked.setup);
    if (!refused.empty()) {
        (void)mail.answer(loom::Refused{refused});
        return;
    }
    settle(mail);
    (void)mail.answer(loom::Ack{});
}

// ---- A pane's row taken off the live layout -----------------------------------------------------

// WL-DESK-12 -- agents/workshop/desktop.md
void LayoutsOwner::on(const PaneCloseRequested& asked, loom::Mail& mail) {
    read_host();
    if (mail.authored_role().empty()) {
        // AN OFFICE, AND ONLY AN OFFICE, changes who is on the desk -- the seam's rule -- and the
        // asker is told so.
        ++state_.refused;
        PaneCloseAnswered out;
        out.office = asked.office;
        out.pane = asked.pane;
        out.refusal = "a pane is taken off the desk by an office -- personal speech closes nothing";
        (void)mail.answer(std::move(out));
        return;
    }
    PaneCloseAnswered out = store_.close(PaneRef{asked.office, asked.pane});
    // THE PICTURE GOES FIRST, with the close's sentence for the presenter's band, so the asker's
    // answer follows a live layout that no longer seats the row.
    settle(mail);
    (void)mail.answer(std::move(out));
}

// ---- What each pane and office takes ------------------------------------------------------------

// WL-SETTING-03 -- agents/workshop/settings.md
void LayoutsOwner::on(const PaneSettingsDeclared& declared, loom::Mail& mail) {
    read_host();
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // personal speech declares nothing (the offer's rule)
    }
    std::vector<v2::PaneSettingRow> rows;
    if (const std::string wrong = pane_settings_declared_problem(declared); !wrong.empty()) {
        // REFUSED ALOUD, AND THE SETTINGS IN FORCE STAND: only an accepted declaration replaces.
        store_.refuse_declaration(store_.name_of(PaneRef{std::string(office), declared.pane}) +
                                  " @" + std::string(office) +
                                  ": its settings were not taken -- " + wrong);
        settle(mail);
        return;
    }
    rows.reserve(declared.rows.size());
    for (const PaneSettingRow& row : declared.rows) {
        rows.push_back(widen_setting_row(row));
    }
    if (const std::string refused =
            store_.declare_pane(office, declared.pane, std::move(rows), mail.sender());
        !refused.empty()) {
        store_.refuse_declaration(refused);
    }
    settle(mail);
}

// WL-SETTING-03 -- agents/workshop/settings.md
void LayoutsOwner::on(const v2::PaneSettingsDeclared& declared, loom::Mail& mail) {
    read_host();
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return;
    }
    if (const std::string refused =
            store_.declare_pane(office, declared.pane, declared.rows, mail.sender());
        !refused.empty()) {
        store_.refuse_declaration(refused);
    }
    settle(mail);
}

// WL-SETTING-10 -- agents/workshop/settings.md
void LayoutsOwner::on(const OfficeSettingsDeclared& declared, loom::Mail& mail) {
    read_host();
    const std::string office(mail.authored_role());
    if (office.empty()) {
        return; // personal speech declares nothing (the offer's rule)
    }
    if (const std::string refused = store_.declare_office(office, declared.rows, mail.sender());
        !refused.empty()) {
        store_.refuse_declaration(refused);
    }
    settle(mail);
}

// ---- A row's settings, handed to its pane -------------------------------------------------------

// WL-SETTING-04 -- agents/workshop/settings.md
void LayoutsOwner::on(const PaneSettingsAsked& asked, loom::Mail& mail) {
    read_host();
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // personal speech asks for no pane's row (the offer's rule)
    }
    // THE LIFE THAT ASKS HOLDS NOTHING IT WAS HANDED: a reloaded image keeps the weave's id, so
    // only its word says so. It is handed its row now, when the live layout seats it.
    store_.pane_asked(PaneRef{std::string(office), asked.pane});
    settle(mail);
}

// WL-SETTING-08 -- agents/workshop/settings.md
void LayoutsOwner::on(const PaneSettingUnused& unused, loom::Mail& mail) {
    read_host();
    const std::string_view office = mail.authored_role();
    if (office.empty()) {
        return; // personal speech says nothing about a pane (the offer's rule)
    }
    store_.keep_unused(office, mail.sender(), unused);
    settle(mail);
}

// ---- The settings doors -------------------------------------------------------------------------

// WL-SETTING-07 -- agents/workshop/settings.md
void LayoutsOwner::on(const PaneSettingsRequested& asked, loom::Mail& mail) {
    read_host();
    const std::string refused = asker_refused(mail);
    state_.withheld += refused.empty() ? 0 : 1;
    settle(mail);
    // READ AFTER THE HAND-OFFS, so the row's sentence says how far its own got.
    PaneSettingsAnswered shown = store_.read_pane(refused, asked);
    (void)mail.answer(std::move(shown));
}

// WL-SETTING-07 -- agents/workshop/settings.md
void LayoutsOwner::on(const PaneSettingCommitRequested& asked, loom::Mail& mail) {
    read_host();
    const std::string refused = asker_refused(mail);
    state_.withheld += refused.empty() ? 0 : 1;
    PaneSettingCommitAnswered acted = store_.commit_pane(refused, asked);
    settle(mail);
    if (acted.accepted && !acted.handed.empty()) {
        // HOW FAR THE WRITE GOT, once its hand-off was asked for.
        acted.handed = store_.handed_fact(PaneRef{asked.provider, asked.pane});
    }
    (void)mail.answer(std::move(acted));
}

// WL-SETTING-10 -- agents/workshop/settings.md
void LayoutsOwner::on(const OfficeSettingsRequested& asked, loom::Mail& mail) {
    read_host();
    const std::string refused = asker_refused(mail);
    state_.withheld += refused.empty() ? 0 : 1;
    OfficeSettingsAnswered shown = store_.read_office(refused, asked);
    settle(mail);
    (void)mail.answer(std::move(shown));
}

// WL-SETTING-10 -- agents/workshop/settings.md
void LayoutsOwner::on(const OfficeSettingCommitRequested& asked, loom::Mail& mail) {
    read_host();
    const std::string refused = asker_refused(mail);
    state_.withheld += refused.empty() ? 0 : 1;
    OfficeSettingCommitAnswered acted = store_.commit_office(refused, asked);
    settle(mail);
    (void)mail.answer(std::move(acted));
}

// WL-SETTING-12 -- agents/workshop/settings.md
void LayoutsOwner::on(const PaneSettingPinRequested& asked, loom::Mail& mail) {
    read_host();
    const std::string refused = asker_refused(mail);
    state_.withheld += refused.empty() ? 0 : 1;
    PaneSettingPinAnswered acted = store_.pin(refused, asked);
    settle(mail);
    (void)mail.answer(std::move(acted));
}

// WL-LOCK-01, WL-LOCK-02 -- agents/workshop/setting-locks.md
void LayoutsOwner::on(const PaneSettingLockRequested& asked, loom::Mail& mail) {
    read_host();
    const std::string refused = asker_refused(mail);
    state_.withheld += refused.empty() ? 0 : 1;
    // AN UNLOCK IS THE WEAVER'S HAND: the Terminal this host mounted, which answers the weaver
    // alone, or an office naming the weaver's current act -- which only the presenter, the judge
    // of every hand, can say. It is asked, and the unlock answered when it answers.
    const std::string office(mail.authored_role());
    if (refused.empty() && asked.act == kUnlockAct && !office.empty() && asked.gesture > 0) {
        PendingUnlock pending;
        pending.question = ++questions_;
        pending.asked = asked;
        pending.refused = refused;
        const loom::Ticket sent =
            mail.as_role(kLayoutsRole)
                .send_to_role(kWorkshopProvider, WeaversActRequested{asked.gesture, office},
                              pending.question);
        if (sent.valid()) {
            ++state_.questions;
            pending.answer = mail.defer_answer();
            unlocks_.push_back(std::move(pending));
            return;
        }
    }
    PaneSettingLockAnswered acted = store_.lock(refused, asked, from_terminal(mail));
    settle(mail);
    (void)mail.answer(std::move(acted));
}

// WL-LOCK-03 -- agents/workshop/setting-locks.md
void LayoutsOwner::on(const LayoutSettingsRequested&, loom::Mail& mail) {
    read_host();
    const std::string refused = asker_refused(mail);
    state_.withheld += refused.empty() ? 0 : 1;
    LayoutSettingsAnswered shown = store_.read_layouts(refused);
    settle(mail);
    (void)mail.answer(std::move(shown));
}

// WL-LOCK-03 -- agents/workshop/setting-locks.md
void LayoutsOwner::on(const SettingsWriteRequested& asked, loom::Mail& mail) {
    read_host();
    const std::string refused = asker_refused(mail);
    state_.withheld += refused.empty() ? 0 : 1;
    SettingsWriteAnswered written = store_.write(refused, asked);
    settle(mail);
    (void)mail.answer(std::move(written));
}

// ---- The grant ----------------------------------------------------------------------------------

loom::Grant layouts_grant() {
    loom::Grant grant;
    // EACH ANSWER, to whoever asked: Loom picks the recipient of an answer.
    grant.allow_to_any(LayoutEditAnswered::zen_name, LayoutEditAnswered::zen_version);
    grant.allow_to_any(SessionSaveAnswered::zen_name, SessionSaveAnswered::zen_version);
    grant.allow_to_any(loom::Ack::zen_name, loom::Ack::zen_version);
    grant.allow_to_any(loom::Refused::zen_name, loom::Refused::zen_version);
    grant.allow_to_any(PaneSettingsAnswered::zen_name, PaneSettingsAnswered::zen_version);
    grant.allow_to_any(PaneSettingCommitAnswered::zen_name, PaneSettingCommitAnswered::zen_version);
    grant.allow_to_any(OfficeSettingsAnswered::zen_name, OfficeSettingsAnswered::zen_version);
    grant.allow_to_any(OfficeSettingCommitAnswered::zen_name,
                       OfficeSettingCommitAnswered::zen_version);
    grant.allow_to_any(PaneSettingPinAnswered::zen_name, PaneSettingPinAnswered::zen_version);
    grant.allow_to_any(PaneSettingLockAnswered::zen_name, PaneSettingLockAnswered::zen_version);
    grant.allow_to_any(LayoutSettingsAnswered::zen_name, LayoutSettingsAnswered::zen_version);
    grant.allow_to_any(SettingsWriteAnswered::zen_name, SettingsWriteAnswered::zen_version);
    grant.allow_to_any(PaneCloseAnswered::zen_name, PaneCloseAnswered::zen_version);
    grant.allow_to_any(manual::ShapeDocumentShown::zen_name, manual::ShapeDocumentShown::zen_version);
    // ...EACH PANE ITS ROW, AND EACH DECLARING OFFICE ITS SETTINGS...
    grant.allow_to_any(PaneSettings::zen_name, PaneSettings::zen_version);
    grant.allow_to_any(v2::PaneSettings::zen_name, v2::PaneSettings::zen_version);
    grant.allow_to_any(OfficeSettings::zen_name, OfficeSettings::zen_version);
    // ...AND THE PRESENTER ITS PICTURE, THE INVENTORY ASKED AND THE ONE QUESTION.
    grant.allow_to_role(LiveLayout::zen_name, LiveLayout::zen_version, kWorkshopProvider);
    grant.allow_to_role(PaneInventoryRequested::zen_name, PaneInventoryRequested::zen_version,
                        kWorkshopProvider);
    grant.allow_to_role(WeaversActRequested::zen_name, WeaversActRequested::zen_version,
                        kWorkshopProvider);
    return grant;
}

} // namespace zengine::workshop
