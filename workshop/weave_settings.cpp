// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s settings doors, each asking the layouts' store: what an office declares it
// takes, one office's settings read and written, Workshop's own applied, every office handed its
// own, a pane's pin list edited, a setting locked in a layout or everywhere, and every layout's
// rows read at once and written in one batch.
// Workshop law: agents/workshop/settings.md, agents/workshop/setting-locks.md

#include "weave.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

// ---- The store, held here ---------------------------------------------------------------------

LayoutStore& WorkshopWeave::store() {
    layouts_host_.setup_path = host_->setup_path;
    layouts_host_.session_path = host_->session_path;
    layouts_host_.prefs_path = host_->prefs_path;
    layouts_host_.conversions = host_->conversions;
    layouts_host_.role_holder = host_->role_holder;
    layouts_host_.incarnation_of = host_->incarnation_of;
    layouts_host_.holder_accepts = host_->holder_accepts;
    layouts_host_.chosen = host_->chosen;
    std::vector<CatalogName> names;
    for (const CatalogRow& row : inventory_rows(session_.setup.active, session_.panes)) {
        names.push_back(CatalogName{row.ref, row.name,
                                    resolve_pane(row.ref, session_.panes).has_value()});
    }
    store_.set_catalog(std::move(names));
    return store_;
}

void WorkshopWeave::take_store(loom::Mail& mail) {
    const StoreEffects fx = store_.take_effects();
    if (fx.live_moved) {
        install_live(store_.live_layout(), mail);
    }
    if (std::find(fx.offices.begin(), fx.offices.end(), kWorkshopProvider) != fx.offices.end()) {
        apply_workshop_settings();
    }
    hand_office_settings(mail);
    if (!fx.said.empty()) {
        say(fx.said, fx.bad);
    }
}

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::hand_office_settings(loom::Mail& mail) {
    for (auto& [office, settings] : store().office_hand_offs()) {
        const loom::Ticket sent =
            mail.as_role(kWorkshopProvider).send_to_role(office, settings);
        if (sent.valid()) {
            store_.office_handed(office, settings.revision);
        }
    }
}

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::apply_workshop_settings() {
    const prefs_persist::Presentation said = prefs_persist::presentation_of(store_.prefs());
    session_.pane_titles = said.titles_shown;
    session_.band_keys = said.band_keys;
    session_.hotkeys_keys = said.hotkeys_keys;
    session_.selection_ring_plain = said.ring_plain;
    session_.skin = said.skin;
    session_.completion_required_first = said.completion_required_first;
    session_.arrange_step_cells = said.arrange_step_cells;
    session_.snap_reach_px = said.snap_reach_px;
    session_.side_cols = said.side_cols;
    session_.stack_rows = said.stack_rows;
}

// ---- What an office declares, and one office's settings read and written ---------------------

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::on(const OfficeSettingsDeclared& declared, loom::Mail& mail) {
    const std::string office(mail.authored_role());
    if (office.empty()) {
        return; // personal speech declares nothing (the offer's rule)
    }
    if (const std::string refused = store().declare_office(office, declared.rows, mail.sender());
        !refused.empty()) {
        say(refused, true);
    }
    take_store(mail);
    repaint(mail);
}

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::on(const OfficeSettingsRequested& asked, loom::Mail& mail) {
    OfficeSettingsShown shown = store().read_office(settings_asker_refused(mail), asked);
    take_store(mail);
    (void)mail.answer(std::move(shown));
}

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::on(const OfficeSettingCommitRequested& asked, loom::Mail& mail) {
    PaneSettingActed acted = store().commit_office(settings_asker_refused(mail), asked);
    take_store(mail);
    (void)mail.answer(std::move(acted));
    repaint(mail);
}

// WL-SETTING-12 -- agents/workshop/settings.md
void WorkshopWeave::on(const PaneSettingPinRequested& asked, loom::Mail& mail) {
    PaneSettingPinActed acted = store().pin(settings_asker_refused(mail), asked);
    take_store(mail);
    (void)mail.answer(std::move(acted));
    repaint(mail);
}

// ---- Locks ------------------------------------------------------------------------------------

// WL-LOCK-02 -- agents/workshop/setting-locks.md
bool WorkshopWeave::weavers_act(std::int64_t gesture, std::string_view office) const {
    if (gesture <= 0 || office.empty() || weaver_hand_.latest == 0) {
        return false;
    }
    // THE WEAVER'S HAND'S OWN RECORD OF AN ACT SENT TO A PANE OF THE ASKING OFFICE, current while
    // nothing of that hand's came since; a guest's act is in the guest's hand, never this one.
    const auto names = [&](const GestureSent& sent) {
        if (sent.answering != static_cast<std::uint64_t>(gesture) ||
            sent.gesture != weaver_hand_.latest) {
            return false;
        }
        const RuntimePane* row = session_.panes.runtime.of_kind(sent.kind);
        return row != nullptr && row->provider == office;
    };
    if (names(weaver_hand_.action_sent) || names(weaver_hand_.press_sent) ||
        names(weaver_hand_.shortcut_sent)) {
        return true;
    }
    return std::any_of(weaver_hand_.kept.begin(), weaver_hand_.kept.end(), names);
}

// WL-LOCK-01, WL-LOCK-02 -- agents/workshop/setting-locks.md
void WorkshopWeave::on(const PaneSettingLockRequested& asked, loom::Mail& mail) {
    // AN UNLOCK IS THE WEAVER'S HAND: the Terminal this host mounted, which answers the weaver
    // alone, or a pane naming the weaver's own current act.
    const bool terminal = host_->terminal != nullptr && mail.sender() == host_->terminal->id();
    const bool weavers_hand = terminal || weavers_act(asked.gesture, mail.authored_role());
    PaneSettingLockActed acted = store().lock(settings_asker_refused(mail), asked, weavers_hand);
    take_store(mail);
    (void)mail.answer(std::move(acted));
    repaint(mail);
}

// ---- Every layout at once ---------------------------------------------------------------------

// WL-LOCK-03 -- agents/workshop/setting-locks.md
void WorkshopWeave::on(const LayoutSettingsRequested&, loom::Mail& mail) {
    LayoutSettingsShown shown = store().read_layouts(settings_asker_refused(mail));
    take_store(mail);
    (void)mail.answer(std::move(shown));
}

// WL-LOCK-03 -- agents/workshop/setting-locks.md
void WorkshopWeave::on(const SettingsWriteRequested& asked, loom::Mail& mail) {
    SettingsWritten written = store().write(settings_asker_refused(mail), asked);
    take_store(mail);
    (void)mail.answer(std::move(written));
    repaint(mail);
}

} // namespace zengine::workshop
