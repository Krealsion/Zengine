// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s own settings, kept by the layouts' owner: declared to it, applied as it hands
// them, and a toggle's value written through it; and the one question it asks Workshop, the judge
// of every hand -- whether a gesture is the weaver's current act.
// Workshop law: agents/workshop/settings.md, agents/workshop/setting-locks.md

#include "weave.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

// ---- Workshop's own role-wide settings ------------------------------------------------------

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::declare_own_settings(loom::Mail& mail) {
    if (own_declared_) {
        return;
    }
    // DECLARED AS ANY ROLE DECLARES ITS OWN, to the role that keeps them; handed back at each
    // revision, they are what this desk spends.
    own_declared_ = mail.as_role(kWorkshopProvider)
                        .send_to_role(kLayoutsRole,
                                      RoleSettingsDeclared{prefs_persist::workshop_role_rows()})
                        .valid();
}

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::apply_workshop_settings(const std::vector<PaneSetting>& settings) {
    prefs_persist::Prefs kept;
    if (!settings.empty()) {
        kept.roles.push_back(prefs_persist::RoleSettingsKept{kWorkshopProvider, settings});
    }
    const prefs_persist::Presentation said = prefs_persist::presentation_of(kept);
    session_.pane_titles = said.titles_shown;
    session_.band_keys = said.band_keys;
    session_.hotkeys_keys = said.hotkeys_keys;
    session_.selection_ring_plain = said.ring_plain;
    session_.completion_required_first = said.completion_required_first;
    session_.arrange_step_cells = said.arrange_step_cells;
    session_.snap_reach_px = said.snap_reach_px;
    session_.stack_rows = said.stack_rows;
    session_.open_at_mouse = said.open_at_mouse;
}

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::on(const RoleSettings& settings, loom::Mail& mail) {
    if (!mail.authored_from_role(kLayoutsRole)) {
        return; // Workshop's settings are the layouts' owner's to hand, and nobody else's
    }
    own_revision_ = settings.revision;
    apply_workshop_settings(settings.settings);
    // A TOGGLE STATED BEFORE THIS DESK HELD A REVISION IS WRITTEN NOW, at the one it holds.
    if (own_owed_) {
        std::pair<RoleSettingCommitRequested, std::string> owed = std::move(*own_owed_);
        own_owed_.reset();
        commit_own_setting(std::move(owed.first), std::move(owed.second), mail);
    }
    repaint(mail);
}

// WL-KEY-16 -- agents/workshop/key-spelling.md
void WorkshopWeave::commit_own_setting(RoleSettingCommitRequested commit, std::string note,
                                       loom::Mail& mail) {
    if (own_revision_ == 0) {
        // WRITTEN WHEN THE OWNER FIRST HANDS THIS DESK ITS OWN; the toggle stands meanwhile.
        own_owed_ = std::pair{std::move(commit), std::move(note)};
        return;
    }
    commit.revision = own_revision_;
    const std::uint64_t ask = ++layout_asks_;
    if (mail.as_role(kWorkshopProvider).send_to_role(kLayoutsRole, std::move(commit), ask).valid()) {
        own_said_[ask] = std::move(note);
        return;
    }
    // NOBODY KEEPS THE LAYOUTS: the toggle stands for this run, and says so.
    say(note + "; not saved: nothing keeps this Workshop's settings", true);
}

// WL-KEY-16 -- agents/workshop/key-spelling.md
void WorkshopWeave::on(const RoleSettingCommitAnswered& acted, loom::Mail& mail) {
    // AN ANSWER IS LOOM'S TO VERIFY, to an ask this weave sent; it is personal speech.
    if (!mail.answers_ask()) {
        return;
    }
    const auto owed = own_said_.find(mail.correlation());
    if (owed == own_said_.end()) {
        return;
    }
    std::string note = std::move(owed->second);
    own_said_.erase(owed);
    bool bad = false;
    if (!host_->prefs_path.empty()) {
        const std::string written_word = "; written";
        if (!acted.accepted) {
            // A FILE THE OWNER COULD NOT READ IS NEVER WRITTEN OVER, and the toggle stands for the
            // run.
            note += "; not saved: " +
                    (settings_refused_.empty()
                         ? acted.refusal
                         : host_->prefs_path + " could not be read and will not be overwritten");
            bad = true;
        } else if (acted.handed.size() < written_word.size() ||
                   acted.handed.compare(acted.handed.size() - written_word.size(),
                                        written_word.size(), written_word) != 0) {
            if (const std::size_t at = acted.handed.find("not written: ");
                at != std::string::npos) {
                note += "; not saved: " + acted.handed.substr(at + 13);
                bad = true;
            }
        }
    }
    say(note, bad);
    repaint(mail);
}

// ---- The weaver's act, judged for the owner ---------------------------------------------------

// WL-LOCK-02 -- agents/workshop/setting-locks.md
bool WorkshopWeave::weavers_act(std::int64_t gesture, std::string_view role) const {
    if (gesture <= 0 || role.empty() || weaver_hand_.latest == 0) {
        return false;
    }
    // THE WEAVER'S HAND'S OWN RECORD OF AN ACT SENT TO A PANE OF THE ASKING ROLE, current while
    // nothing of that hand's came since; a guest's act is in the guest's hand, never this one.
    const auto names = [&](const GestureSent& sent) {
        if (sent.answering != static_cast<std::uint64_t>(gesture) ||
            sent.gesture != weaver_hand_.latest) {
            return false;
        }
        const RuntimePane* row = session_.panes.runtime.of_kind(sent.kind);
        return row != nullptr && row->provider == role;
    };
    if (names(weaver_hand_.action_sent) || names(weaver_hand_.press_sent) ||
        names(weaver_hand_.shortcut_sent)) {
        return true;
    }
    return std::any_of(weaver_hand_.kept.begin(), weaver_hand_.kept.end(), names);
}

// WL-LOCK-02 -- agents/workshop/setting-locks.md
void WorkshopWeave::on(const WeaversActRequested& asked, loom::Mail& mail) {
    if (!mail.authored_from_role(kLayoutsRole)) {
        return; // the owner's question, and nobody else's: the judge answers no other asker
    }
    (void)mail.answer(WeaversActAnswered{weavers_act(asked.gesture, asked.role)});
}

} // namespace zengine::workshop
