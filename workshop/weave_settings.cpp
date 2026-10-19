// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s office-wide settings, pins and locks: what an office declares it takes, one
// office's settings read and written by message, Workshop's own applied, every office handed its
// own, the settings file written only while its bytes are the ones this host read, a pane's pin
// list edited in the list the pin scope reads, a setting locked in a layout or everywhere, and every
// layout's rows read at once and written in one batch.
// Workshop law: agents/workshop/settings.md, agents/workshop/setting-locks.md

#include "weave.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

namespace {

/// A pane's Workshop-wide pin list target, `<office>/<pane>`.
std::string pane_target(const PaneRef& ref) { return ref.provider + "/" + ref.pane; }

/// Whether `key` is in `keys`.
bool holds(const std::vector<std::string>& keys, const std::string& key) {
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

/// The pane a `<office>/<pane>` target names, or none for any other spelling.
std::optional<PaneRef> pane_of_target(std::string_view target) {
    const std::size_t slash = target.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 == target.size() ||
        target.find('/', slash + 1) != std::string_view::npos) {
        return std::nullopt;
    }
    return PaneRef{std::string(target.substr(0, slash)), std::string(target.substr(slash + 1))};
}

/// The desk at shelf position `at`, to write: the live one, or a shelved one in place.
Setup& desk_at(SetupState& s, std::size_t at) {
    if (at == s.active_at || at >= layout_count(s)) {
        return s.active;
    }
    return s.shelved[shelf_index(s, at)].desk;
}

/// The declared row under `key` in `rows`, or nullptr.
const v2::PaneSettingRow* row_named(const std::vector<v2::PaneSettingRow>* rows,
                                    std::string_view key) {
    for (std::size_t i = 0; rows != nullptr && i < rows->size(); ++i) {
        if ((*rows)[i].key == key) {
            return &(*rows)[i];
        }
    }
    return nullptr;
}

/// GIVE `ref`'S ROW OF `desk` THE VALUE AN EVERYWHERE LOCK PINS -- its default for `clear` --
/// through the setter, judged by the row's and the desk's bounds. A lock pinning nothing, or a desk
/// not seating the pane, is left as it is.
Written pin_lock_value(Setup& desk, const PaneRef& ref, const SettingLock& lock) {
    const SetupPane* row = pane_of(desk, ref);
    if (row == nullptr || (!lock.clear && setting_kind(lock) == SettingKind::kNone)) {
        return Written::ok();
    }
    const PaneSetting* now = find_pane_setting(row->settings, lock.key);
    if (lock.clear) {
        return now == nullptr ? Written::ok()
                              : author_pane_setting(desk, ref, lock.key, std::nullopt, std::nullopt);
    }
    const PaneSetting pinned{lock.key, lock.flag, lock.number, lock.text};
    if (now != nullptr && *now == pinned) {
        return Written::ok();
    }
    return author_pane_setting(desk, ref, lock.key, pinned, std::nullopt);
}

} // namespace

// ---- What an office declares --------------------------------------------------------------

// WL-SETTING-10 -- agents/workshop/settings.md
const std::vector<v2::PaneSettingRow>* WorkshopWeave::office_rows(std::string_view office) const {
    static const std::vector<v2::PaneSettingRow> own = prefs_persist::workshop_office_rows();
    if (office == kWorkshopProvider) {
        return &own;
    }
    for (const OfficeDeclaration& d : office_declarations_) {
        if (d.office != office) {
            continue;
        }
        // COUNTED WHILE THE INCARNATION THAT SENT IT HOLDS THE OFFICE, as a pane's declaration is.
        if (!d.from.valid() || !host_->role_holder || host_->role_holder(d.office) != d.from) {
            return nullptr;
        }
        if (host_->incarnation_of && host_->incarnation_of(d.from) != d.incarnation) {
            return nullptr;
        }
        return &d.rows;
    }
    return nullptr;
}

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::on(const OfficeSettingsDeclared& declared, loom::Mail& mail) {
    const std::string office(mail.authored_role());
    if (office.empty()) {
        return; // personal speech declares nothing (the offer's rule)
    }
    const std::string at = "office-wide settings of " + office + ": ";
    if (office == kWorkshopProvider) {
        say(at + "refused -- Workshop's own are declared in its code", true);
        repaint(mail);
        return;
    }
    // JUDGED WHOLE BY A PANE DECLARATION'S RULES; only an accepted one replaces what stands.
    if (const std::string wrong =
            v2::pane_settings_declared_problem(v2::PaneSettingsDeclared{std::string(), declared.rows});
        !wrong.empty()) {
        say(at + "refused -- " + wrong, true);
        repaint(mail);
        return;
    }
    auto kept = std::find_if(office_declarations_.begin(), office_declarations_.end(),
                             [&](const OfficeDeclaration& d) { return d.office == office; });
    if (kept == office_declarations_.end()) {
        // ONE PLACE IS WORKSHOP'S OWN, so the bound counts it.
        if (office_declarations_.size() + 2 > kMaxSettingOffices) {
            say(at + "refused -- at most " + std::to_string(kMaxSettingOffices) +
                    " offices declare office-wide settings, Workshop's own among them",
                true);
            repaint(mail);
            return;
        }
        office_declarations_.push_back(OfficeDeclaration{office, {}, {}, 0, 0});
        kept = office_declarations_.end() - 1;
    }
    kept->rows = declared.rows;
    kept->from = mail.sender();
    kept->incarnation = host_->incarnation_of ? host_->incarnation_of(mail.sender()) : 0;
    kept->handed = 0;
    hand_office_settings(mail);
    repaint(mail);
}

// WL-SETTING-10 -- agents/workshop/settings.md
std::int64_t WorkshopWeave::office_revision(const std::string& office) {
    const std::vector<v2::PaneSettingRow>* rows = office_rows(office);
    if (rows == nullptr) {
        return 0;
    }
    const prefs_persist::OfficeSettingsKept* kept = prefs_persist::office_kept(prefs_, office);
    OfficeRevision now{office,
                       0,
                       *rows,
                       kept != nullptr ? kept->settings : std::vector<PaneSetting>{},
                       office == kWorkshopProvider || !host_->role_holder
                           ? loom::WeaveId{}
                           : host_->role_holder(office)};
    for (OfficeRevision& r : office_revisions_) {
        if (r.office != office) {
            continue;
        }
        if (r.rows != now.rows || r.settings != now.settings || r.holder != now.holder) {
            now.revision = ++settings_revision_;
            r = std::move(now);
        }
        return r.revision;
    }
    now.revision = ++settings_revision_;
    office_revisions_.push_back(std::move(now));
    return office_revisions_.back().revision;
}

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::hand_office_settings(loom::Mail& mail) {
    if (!host_->holder_accepts || !host_->role_holder) {
        return;
    }
    for (OfficeDeclaration& d : office_declarations_) {
        if (office_rows(d.office) == nullptr ||
            !host_->holder_accepts(d.office, *loom::schema_of<OfficeSettings>())) {
            continue;
        }
        const std::int64_t revision = office_revision(d.office);
        if (d.handed == revision) {
            continue; // what it holds is what is kept: saying so again is noise
        }
        const prefs_persist::OfficeSettingsKept* kept = prefs_persist::office_kept(prefs_, d.office);
        const loom::Ticket sent = mail.as_role(kWorkshopProvider)
                                      .send_to_role(d.office,
                                                    OfficeSettings{revision,
                                                                   kept != nullptr
                                                                       ? kept->settings
                                                                       : std::vector<PaneSetting>{}});
        if (sent.valid()) {
            d.handed = revision;
        }
    }
}

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::apply_workshop_settings() {
    const prefs_persist::Presentation said = prefs_persist::presentation_of(prefs_);
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

// WL-SETTING-11 -- agents/workshop/settings.md
std::string WorkshopWeave::write_prefs() {
    if (host_->prefs_path.empty()) {
        return "isolated run -- no settings file; the change stands for this run";
    }
    if (prefs_bad_) {
        return host_->prefs_path + " could not be read and will not be overwritten";
    }
    // ONLY WHILE ITS BYTES ARE THE ONES THIS HOST READ OR WROTE: another hand's change is seen,
    // never overwritten.
    std::error_code ec;
    const bool exists = std::filesystem::exists(host_->prefs_path, ec);
    bool changed = exists != prefs_file_present_;
    if (!changed && exists) {
        const persist::FileText now = persist::read_file(
            host_->prefs_path, prefs_persist::kMaxPrefsBytes, "a Workshop prefs file");
        changed = !now.outcome.accepted || now.text != prefs_bytes_;
    }
    if (changed) {
        return "the settings file changed since this Workshop read it -- not written; relaunch to "
               "read the file, then change it again";
    }
    const std::string text = prefs_persist::to_text(prefs_);
    const Written wrote = persist::write_file_making_room(host_->prefs_path, text);
    if (!wrote.accepted) {
        return wrote.refusal;
    }
    prefs_bytes_ = text;
    prefs_file_present_ = true;
    return std::string();
}

// ---- One office's settings, read and written by message -------------------------------------

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::on(const OfficeSettingsRequested& asked, loom::Mail& mail) {
    load_prefs();
    OfficeSettingsShown shown;
    shown.office = asked.office;
    if (const std::string refused = settings_asker_refused(mail); !refused.empty()) {
        shown.written = "settings withheld: " + refused;
        (void)mail.answer(std::move(shown));
        return;
    }
    if (const std::vector<v2::PaneSettingRow>* rows = office_rows(asked.office)) {
        shown.rows = *rows;
    }
    if (const prefs_persist::OfficeSettingsKept* kept =
            prefs_persist::office_kept(prefs_, asked.office)) {
        shown.settings = kept->settings;
    }
    for (const prefs_persist::SettingLock& l : prefs_.locks) {
        if (l.target == asked.office) {
            shown.locks.push_back(SettingLockShown{l.key, kLockedEverywhere});
        }
    }
    shown.revision = office_revision(asked.office);
    if (host_->prefs_path.empty()) {
        shown.written = "isolated run -- no settings file";
    } else if (prefs_bad_) {
        shown.written = "refused at launch: " + host_->prefs_path +
                        " could not be read and will not be overwritten";
    } else if (prefs_file_present_ ? prefs_bytes_ == prefs_persist::to_text(prefs_)
                                   : prefs_ == prefs_persist::Prefs{}) {
        shown.written = "written";
    } else {
        shown.written = "not written: the file does not hold what is kept";
    }
    (void)mail.answer(std::move(shown));
}

// WL-SETTING-10 -- agents/workshop/settings.md
void WorkshopWeave::on(const OfficeSettingCommitRequested& asked, loom::Mail& mail) {
    load_prefs();
    const auto refuse = [&mail](std::string why, std::int64_t revision) {
        (void)mail.answer(PaneSettingActed{false, std::move(why), revision, std::string()});
    };
    // IN ORDER, EACH REFUSAL WRITING NOTHING: the asker, the office, the revision, the file, the
    // key, the lock, then the value.
    if (const std::string refused = settings_asker_refused(mail); !refused.empty()) {
        refuse("settings commit withheld: " + refused, 0);
        return;
    }
    const std::vector<v2::PaneSettingRow>* rows = office_rows(asked.office);
    if (rows == nullptr) {
        refuse("nothing declares office-wide settings for `" + asked.office + "` here", 0);
        return;
    }
    const std::int64_t current = office_revision(asked.office);
    if (asked.revision != current) {
        refuse("stale revision " + std::to_string(asked.revision) + " -- the settings of " +
                   asked.office + " stand at " + std::to_string(current) + "; read them again",
               current);
        return;
    }
    if (prefs_bad_) {
        refuse("Workshop's settings file " + host_->prefs_path +
                   " could not be read at launch and is never written over -- mend or remove it, "
                   "then relaunch",
               current);
        return;
    }
    const v2::PaneSettingRow* declared = nullptr;
    for (const v2::PaneSettingRow& row : *rows) {
        declared = row.key == asked.key ? &row : declared;
    }
    const int fields = (asked.flag ? 1 : 0) + (asked.number ? 1 : 0) + (asked.text ? 1 : 0);
    if (asked.clear && fields != 0) {
        refuse("a clear takes no value -- name the key alone", current);
        return;
    }
    if (declared == nullptr &&
        !(asked.clear && prefs_persist::office_value(prefs_, asked.office, asked.key) != nullptr)) {
        refuse(asked.office + " declares no setting `" + asked.key +
                   "` -- a key no declaration takes is only cleared",
               current);
        return;
    }
    if (prefs_persist::lock_on(prefs_, asked.office, asked.key) != nullptr) {
        refuse(locked_setting_refusal(asked.key), current);
        return;
    }
    std::optional<PaneSetting> value;
    if (!asked.clear) {
        const PaneSetting candidate{asked.key, asked.flag, asked.number, asked.text};
        if (const std::string wrong = v2::pane_setting_refused_by(*declared, candidate);
            !wrong.empty()) {
            refuse(wrong, current);
            return;
        }
        // THE DEFAULT IS KEPT AS ITS ABSENCE.
        if (!(candidate == PaneSetting{declared->key, declared->flag, declared->number,
                                       declared->text})) {
            value = candidate;
        }
    }
    const prefs_persist::Prefs before = prefs_;
    (void)prefs_persist::put_office_value(prefs_, asked.office, asked.key, value);
    // WITHIN THE FILE'S BOUNDS, so the file written is one the next launch reads.
    if (prefs_persist::kept_count(prefs_) > kMaxOfficeSettingsKept ||
        prefs_persist::office_count(prefs_) > kMaxSettingOffices) {
        prefs_ = before;
        refuse("Workshop keeps at most " + std::to_string(kMaxOfficeSettingsKept) +
                   " office-wide settings, for at most " + std::to_string(kMaxSettingOffices) +
                   " offices",
               current);
        return;
    }
    if (prefs_ == before) {
        // NOTHING MOVED, so nothing is minted, written or handed.
        (void)mail.answer(PaneSettingActed{true, std::string(), current, "unchanged"});
        return;
    }
    // APPLIED OR HANDED, THEN WRITTEN, AND THE TWO SAID APART.
    std::string applied;
    if (asked.office == kWorkshopProvider) {
        apply_workshop_settings();
        applied = "applied";
    } else {
        hand_office_settings(mail);
        applied = host_->holder_accepts &&
                          host_->holder_accepts(asked.office, *loom::schema_of<OfficeSettings>())
                      ? "handed"
                      : "kept; not handed: " + asked.office + " takes no office-wide settings";
    }
    const std::string not_written = write_prefs();
    const std::string handed =
        applied + (not_written.empty() ? "; written" : "; not written: " + not_written);
    say((asked.clear ? "cleared " + asked.key : "committed " + asked.key + " = " +
                                                    setting_value_text(PaneSetting{
                                                        asked.key, asked.flag, asked.number,
                                                        asked.text})) +
            " of " + asked.office + "; " + handed,
        !not_written.empty() && !host_->prefs_path.empty());
    (void)mail.answer(
        PaneSettingActed{true, std::string(), office_revision(asked.office), handed});
    repaint(mail);
}

// ---- Pins -------------------------------------------------------------------------------------

// WL-SETTING-12 -- agents/workshop/settings.md
std::string WorkshopWeave::pin_target(const PaneRef& ref, std::int64_t layout) const {
    if (!prefs_persist::presentation_of(prefs_).pins_per_layout) {
        return pane_target(ref);
    }
    const std::size_t at = layout < 0 ? session_.setup.active_at : static_cast<std::size_t>(layout);
    return pane_target(ref) + "@" + layout_at(session_.setup, at).name;
}

// WL-SETTING-12 -- agents/workshop/settings.md
std::vector<std::string> WorkshopWeave::effective_pins(const PaneRef& ref,
                                                       std::int64_t layout) const {
    // THE SCOPE'S OWN LIST, ELSE, PER LAYOUT, THE WORKSHOP-WIDE ONE, ELSE THE DECLARED PINS: a
    // reading falls back, and no list is written by reading it.
    if (const prefs_persist::PinList* own = prefs_persist::pin_list(prefs_, pin_target(ref, layout))) {
        return own->keys;
    }
    if (const prefs_persist::PinList* wide = prefs_persist::pin_list(prefs_, pane_target(ref))) {
        return wide->keys;
    }
    std::vector<std::string> declared;
    const RuntimePane* offered = session_.panes.runtime.find(ref.provider, ref.pane);
    if (const std::vector<v2::PaneSettingRow>* rows =
            offered != nullptr ? counted_settings_v2(*offered) : nullptr) {
        for (const v2::PaneSettingRow& row : *rows) {
            if (row.pinned) {
                declared.push_back(row.key);
            }
        }
    }
    return declared;
}

// ---- Locks --------------------------------------------------------------------------------------

// WL-LOCK-01 -- agents/workshop/setting-locks.md
std::vector<SettingLockShown> WorkshopWeave::locks_on(const PaneRef& ref, std::size_t layout) const {
    std::vector<SettingLockShown> out;
    const std::string target = pane_target(ref);
    const std::string here =
        prefs_persist::layout_target(target, layout_at(session_.setup, layout).name);
    for (const SettingLock& l : prefs_.locks) {
        if (l.target == target) {
            out.push_back(SettingLockShown{l.key, kLockScopeEverywhere});
        } else if (l.target == here && prefs_persist::lock_on(prefs_, target, l.key) == nullptr) {
            out.push_back(SettingLockShown{l.key, kLockScopeLayout});
        }
    }
    std::sort(out.begin(), out.end(), [](const SettingLockShown& a, const SettingLockShown& b) {
        return a.key < b.key;
    });
    return out;
}

std::vector<std::string> WorkshopWeave::locked_keys(const PaneRef& ref) const {
    std::vector<std::string> out;
    for (const SettingLockShown& l : locks_on(ref, session_.setup.active_at)) {
        out.push_back(l.key);
    }
    return out;
}

std::vector<std::string> WorkshopWeave::layout_locked_keys(const PaneRef& ref) const {
    std::vector<std::string> out;
    for (const SettingLockShown& l : locks_on(ref, session_.setup.active_at)) {
        if (l.scope == kLockScopeLayout) {
            out.push_back(l.key);
        }
    }
    return out;
}

// WL-LOCK-01 -- agents/workshop/setting-locks.md
bool WorkshopWeave::pin_everywhere_locks(Setup& desk) const {
    bool moved = false;
    for (const SettingLock& l : prefs_.locks) {
        const std::optional<PaneRef> ref =
            l.target.find('@') == std::string::npos ? pane_of_target(l.target) : std::nullopt;
        const SetupPane* row = ref ? pane_of(desk, *ref) : nullptr;
        if (row == nullptr) {
            continue;
        }
        const std::vector<PaneSetting> before = row->settings;
        // A ROW PAST ITS BOUNDS KEEPS WHAT IT HAD: the lock still refuses every write to the key.
        (void)pin_lock_value(desk, *ref, l);
        moved = moved || pane_of(desk, *ref)->settings != before;
    }
    return moved;
}

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
    load_prefs();
    const auto refuse = [&mail](std::string why) {
        (void)mail.answer(PaneSettingLockActed{false, std::move(why), {}});
    };
    // IN ORDER, EACH REFUSAL CHANGING NOTHING: the asker, the act, where it holds, the unlocking
    // hand, the key and the target, the layout, the file, then the act on the target.
    if (const std::string refused = settings_asker_refused(mail); !refused.empty()) {
        refuse("lock withheld: " + refused);
        return;
    }
    const bool locking = asked.act == kLockAct;
    if (!locking && asked.act != kUnlockAct) {
        refuse("`" + asked.act + "` is not a lock act -- lock or unlock");
        return;
    }
    const bool everywhere = asked.scope == kLockScopeEverywhere;
    if (!everywhere && asked.scope != kLockScopeLayout) {
        refuse("`" + asked.scope + "` is not where a lock holds -- layout or everywhere");
        return;
    }
    if (!locking) {
        // AN UNLOCK IS THE WEAVER'S HAND: the Terminal this host mounted, which answers the weaver
        // alone, or a pane naming the weaver's own current act.
        const bool terminal = host_->terminal != nullptr && mail.sender() == host_->terminal->id();
        if (!terminal && !weavers_act(asked.gesture, mail.authored_role())) {
            refuse("unlocking is the weaver's -- from the Terminal, or from a pane on the weaver's "
                   "own current act");
            return;
        }
    }
    if (const std::string wrong = pane_setting_key_problem(asked.key); !wrong.empty()) {
        refuse(wrong);
        return;
    }
    if (asked.target.find('@') != std::string::npos) {
        refuse("a lock names its layout by `layout`, never in its target");
        return;
    }
    if (const std::string wrong = prefs_persist::target_problem(asked.target, "a lock's target");
        !wrong.empty()) {
        refuse(wrong);
        return;
    }
    if (asked.layout < -1 ||
        asked.layout >= static_cast<std::int64_t>(layout_count(session_.setup))) {
        refuse("no layout at place " + std::to_string(asked.layout) + " -- this Workshop holds " +
               std::to_string(layout_count(session_.setup)));
        return;
    }
    if (prefs_bad_) {
        refuse("locks not changed -- Workshop's settings file " + host_->prefs_path +
               " could not be read at launch and is never written over");
        return;
    }
    const std::size_t at =
        asked.layout < 0 ? session_.setup.active_at : static_cast<std::size_t>(asked.layout);
    const std::string& target = asked.target;
    const std::string& key = asked.key;
    const std::string layout_name = layout_at(session_.setup, at).name;
    const std::string in_layout = "layout " + quoted_setup_name(layout_name);
    // WHAT THE ACT CHANGES STANDS ONLY ONCE THE SETTINGS FILE HOLDS IT.
    const prefs_persist::Prefs prefs_before = prefs_;
    const SetupState setup_before = session_.setup;
    std::vector<std::string> written;
    std::string done;
    if (target.find('/') == std::string::npos) {
        // AN OFFICE-WIDE SETTING HAS ONE VALUE, so a lock on it holds everywhere and pins nothing.
        const std::vector<v2::PaneSettingRow>* rows = office_rows(target);
        if (rows == nullptr) {
            refuse("nothing declares office-wide settings for `" + target + "` here");
            return;
        }
        if (!everywhere) {
            refuse("an office-wide setting has one value in every layout -- lock it everywhere");
            return;
        }
        if (locking) {
            if (row_named(rows, key) == nullptr &&
                prefs_persist::office_value(prefs_, target, key) == nullptr) {
                refuse(target + " declares no setting `" + key + "` to lock");
                return;
            }
            if (prefs_persist::lock_on(prefs_, target, key) != nullptr) {
                refuse("`" + key + "` of " + target + " is locked already");
                return;
            }
            if (prefs_.locks.size() >= prefs_persist::kMaxLocks) {
                refuse("Workshop keeps at most " + std::to_string(prefs_persist::kMaxLocks) +
                       " locks -- unlock one first");
                return;
            }
            prefs_persist::put_lock(prefs_, SettingLock{target, key});
            done = "locked " + key + " of " + target + " everywhere";
        } else {
            if (!prefs_persist::drop_lock(prefs_, target, key)) {
                refuse("`" + key + "` of " + target + " is not locked");
                return;
            }
            done = "unlocked " + key + " of " + target;
        }
    } else {
        const std::optional<PaneRef> ref = pane_of_target(target);
        if (!ref) {
            refuse("a lock's target is `<office>/<pane>`, or `<office>` for an office-wide setting");
            return;
        }
        const std::string here = prefs_persist::layout_target(target, layout_name);
        const SetupPane* row = pane_of(layout_at(session_.setup, at), *ref);
        const RuntimePane* offered = session_.panes.runtime.find(ref->provider, ref->pane);
        const bool declared =
            key == kWorkshopSkinKey ||
            row_named(offered != nullptr ? counted_settings_v2(*offered) : nullptr, key) != nullptr;
        const PaneSetting* stored = row != nullptr ? find_pane_setting(row->settings, key) : nullptr;
        if (locking && prefs_persist::lock_on(prefs_, target, key) != nullptr) {
            refuse("`" + key + "` of " + target + " is locked everywhere already");
            return;
        }
        if (locking && !declared && stored == nullptr) {
            refuse(target + " neither declares nor keeps `" + key + "` in " + in_layout);
            return;
        }
        if (locking && prefs_.locks.size() >= prefs_persist::kMaxLocks) {
            refuse("Workshop keeps at most " + std::to_string(prefs_persist::kMaxLocks) +
                   " locks -- unlock one first");
            return;
        }
        if (locking && !everywhere) {
            if (row == nullptr) {
                refuse(target + " is not in " + in_layout + " -- a layout's lock holds its row");
                return;
            }
            if (prefs_persist::lock_on(prefs_, here, key) != nullptr) {
                refuse("`" + key + "` of " + target + " is locked in " + in_layout + " already");
                return;
            }
            prefs_persist::put_lock(prefs_, SettingLock{here, key});
            written = {layout_name};
            done = "locked " + key + " of " + target + " in " + in_layout;
        } else if (locking) {
            // AN EVERYWHERE LOCK PINS THE VALUE ITS LAYOUT HOLDS, its default when it keeps none, and
            // takes the place of each layout's own lock on the key.
            SettingLock lock{target, key};
            if (stored != nullptr) {
                lock.flag = stored->flag;
                lock.number = stored->number;
                lock.text = stored->text;
            } else {
                lock.clear = true;
            }
            std::erase_if(prefs_.locks, [&](const SettingLock& l) {
                return l.key == key && l.target.rfind(target + "@", 0) == 0;
            });
            prefs_persist::put_lock(prefs_, lock);
            // ...AND IS WRITTEN INTO EVERY ROW THAT SEATS THE PANE, each judged by its bounds, all
            // or none.
            SetupState next = session_.setup;
            for (std::size_t i = 0; i < layout_count(next); ++i) {
                Setup& desk = desk_at(next, i);
                if (pane_of(desk, *ref) == nullptr) {
                    continue;
                }
                if (const Written w = pin_lock_value(desk, *ref, lock); !w.accepted) {
                    prefs_ = prefs_before;
                    refuse("`" + key + "` of " + target + " cannot be pinned in layout " +
                           quoted_setup_name(desk.name) + ": " + w.refusal);
                    return;
                }
                written.push_back(desk.name);
            }
            session_.setup = std::move(next);
            done = "locked " + key + " of " + target + " everywhere at " +
                   (lock.clear ? std::string("its default")
                               : setting_value_text(PaneSetting{key, lock.flag, lock.number,
                                                                lock.text}));
        } else if (!everywhere) {
            if (!prefs_persist::drop_lock(prefs_, here, key)) {
                refuse(prefs_persist::lock_on(prefs_, target, key) != nullptr
                           ? "`" + key + "` of " + target +
                                 " is locked everywhere -- unlock it everywhere"
                           : "`" + key + "` of " + target + " is not locked in " + in_layout);
                return;
            }
            written = {layout_name};
            done = "unlocked " + key + " of " + target + " in " + in_layout;
        } else {
            if (!prefs_persist::drop_lock(prefs_, target, key)) {
                refuse("`" + key + "` of " + target + " is not locked everywhere");
                return;
            }
            done = "unlocked " + key + " of " + target + " everywhere";
        }
    }
    if (const std::string not_written = write_prefs();
        !not_written.empty() && !host_->prefs_path.empty()) {
        prefs_ = prefs_before;
        session_.setup = setup_before;
        refuse("nothing changed: Workshop's settings file was not written -- " + not_written);
        return;
    }
    // A LIVE ROW AN EVERYWHERE LOCK WROTE IS HANDED NOW, as a commit's is.
    hand_settings(mail);
    say(done, false);
    (void)mail.answer(PaneSettingLockActed{true, std::string(), std::move(written)});
    repaint(mail);
}

// ---- Every layout at once -----------------------------------------------------------------------

// WL-LOCK-03 -- agents/workshop/setting-locks.md
LayoutSettingsShown WorkshopWeave::layout_settings_reading() const {
    LayoutSettingsShown out;
    out.active = static_cast<std::int64_t>(session_.setup.active_at);
    const std::vector<Layout> run = layout_run(session_.setup);
    for (std::size_t i = 0; i < run.size(); ++i) {
        LayoutSettings layout;
        layout.position = static_cast<std::int64_t>(i);
        layout.name = run[i].desk.name;
        layout.theme = prefs_persist::layout_theme_of(prefs_, layout.name);
        for (const SetupPane& p : run[i].desk.panes) {
            LayoutPaneSettings row{p.ref.provider, p.ref.pane, p.settings, {}};
            for (const SettingLockShown& l : locks_on(p.ref, i)) {
                row.locked.push_back(l.key);
            }
            layout.panes.push_back(std::move(row));
        }
        out.layouts.push_back(std::move(layout));
    }
    // WHAT EACH PANE TAKES, Workshop's word for its area first, and what each office takes.
    for (const RuntimePane& r : session_.panes.runtime.entries) {
        const std::vector<v2::PaneSettingRow>* rows = counted_settings_v2(r);
        if (rows == nullptr) {
            continue;
        }
        DeclaredSettings d{r.provider, r.pane, {workshop_skin_setting_row()}};
        for (const v2::PaneSettingRow& row : *rows) {
            if (row.key != kWorkshopSkinKey) {
                d.rows.push_back(row);
            }
        }
        out.declared.push_back(std::move(d));
    }
    out.declared.push_back(DeclaredSettings{kWorkshopProvider, std::string(),
                                            *office_rows(kWorkshopProvider)});
    for (const OfficeDeclaration& d : office_declarations_) {
        if (const std::vector<v2::PaneSettingRow>* rows = office_rows(d.office)) {
            out.declared.push_back(DeclaredSettings{d.office, std::string(), *rows});
        }
    }
    for (const SettingLock& l : prefs_.locks) {
        if (l.target.find('@') == std::string::npos) {
            out.everywhere.push_back(l);
        }
    }
    return out;
}

// WL-LOCK-03 -- agents/workshop/setting-locks.md
std::int64_t WorkshopWeave::layout_settings_revision() {
    LayoutSettingsShown now = layout_settings_reading();
    if (layout_settings_revision_ == 0 || !(now == layout_settings_minted_)) {
        layout_settings_minted_ = std::move(now);
        ++layout_settings_revision_;
    }
    return layout_settings_revision_;
}

// WL-LOCK-03 -- agents/workshop/setting-locks.md
void WorkshopWeave::on(const LayoutSettingsRequested&, loom::Mail& mail) {
    load_prefs();
    if (const std::string refused = settings_asker_refused(mail); !refused.empty()) {
        (void)mail.answer(LayoutSettingsShown{}); // nothing read, at the revision none stands at
        return;
    }
    const std::int64_t revision = layout_settings_revision();
    LayoutSettingsShown shown = layout_settings_minted_;
    shown.revision = revision;
    (void)mail.answer(std::move(shown));
}

// WL-LOCK-03 -- agents/workshop/setting-locks.md
void WorkshopWeave::on(const SettingsWriteRequested& asked, loom::Mail& mail) {
    load_prefs();
    const auto refuse = [&mail](std::string why, std::int64_t revision) {
        (void)mail.answer(SettingsWritten{false, std::move(why), revision, {}, {}});
    };
    // JUDGED WHOLE, NOTHING WRITTEN ON A REFUSAL: the asker, the revision, the bounds, then every
    // write's form, target, layout and value, and every baseline's.
    if (const std::string refused = settings_asker_refused(mail); !refused.empty()) {
        refuse("settings writes withheld: " + refused, 0);
        return;
    }
    const std::int64_t current = layout_settings_revision();
    if (asked.revision != current) {
        refuse("stale revision " + std::to_string(asked.revision) + " -- the layouts stand at " +
                   std::to_string(current) + "; read them again",
               current);
        return;
    }
    const std::size_t layouts = layout_count(session_.setup);
    if (asked.writes.size() > kMaxLayouts * kMaxPaneSettingsPerDesk ||
        asked.baselines.size() > kMaxLayouts) {
        refuse("a batch holds at most " + std::to_string(kMaxLayouts * kMaxPaneSettingsPerDesk) +
                   " writes and " + std::to_string(kMaxLayouts) + " baselines",
               current);
        return;
    }
    const auto place_of = [&](std::int64_t layout) -> std::optional<std::size_t> {
        if (layout < -1 || layout >= static_cast<std::int64_t>(layouts)) {
            return std::nullopt;
        }
        return layout < 0 ? session_.setup.active_at : static_cast<std::size_t>(layout);
    };
    const auto skin_row = workshop_skin_setting_row();
    const auto declared_row = [&](const PaneRef& ref,
                                  const std::string& key) -> const v2::PaneSettingRow* {
        if (key == kWorkshopSkinKey) {
            return &skin_row;
        }
        const RuntimePane* offered = session_.panes.runtime.find(ref.provider, ref.pane);
        return row_named(offered != nullptr ? counted_settings_v2(*offered) : nullptr, key);
    };
    for (std::size_t i = 0; i < asked.writes.size(); ++i) {
        const SettingWrite& w = asked.writes[i];
        const std::string at = "write " + std::to_string(i) + ": ";
        const int fields = (w.flag ? 1 : 0) + (w.number ? 1 : 0) + (w.text ? 1 : 0);
        const PaneSetting candidate{w.key, w.flag, w.number, w.text};
        std::string wrong = pane_setting_key_problem(w.key);
        if (wrong.empty() && w.target.find('@') != std::string::npos) {
            wrong = "a write names its layout by `layout`, never in its target";
        }
        if (wrong.empty()) {
            wrong = prefs_persist::target_problem(w.target, "a write's target");
        }
        if (wrong.empty() && w.clear && fields != 0) {
            wrong = "a clear takes no value -- name the key alone";
        }
        if (wrong.empty() && !w.clear) {
            wrong = pane_setting_problem(candidate);
        }
        const std::optional<PaneRef> ref = pane_of_target(w.target);
        if (wrong.empty() && w.target.find('/') == std::string::npos) {
            if (office_rows(w.target) == nullptr) {
                wrong = "nothing declares office-wide settings for `" + w.target + "` here";
            } else if (prefs_bad_) {
                wrong = "Workshop's settings file " + host_->prefs_path +
                        " could not be read at launch and is never written over";
            } else if (const v2::PaneSettingRow* row = row_named(office_rows(w.target), w.key);
                       row != nullptr && !w.clear) {
                wrong = v2::pane_setting_refused_by(*row, candidate);
            }
        } else if (wrong.empty() && !ref) {
            wrong = "a write's target is `<office>/<pane>`, or `<office>` for an office-wide "
                    "setting";
        } else if (wrong.empty() && !place_of(w.layout)) {
            wrong = "no layout at place " + std::to_string(w.layout) + " -- this Workshop holds " +
                    std::to_string(layouts);
        } else if (wrong.empty() && !w.clear) {
            if (const v2::PaneSettingRow* row = declared_row(*ref, w.key)) {
                wrong = v2::pane_setting_refused_by(*row, candidate);
            }
        }
        if (!wrong.empty()) {
            refuse(at + wrong, current);
            return;
        }
    }
    for (std::size_t i = 0; i < asked.baselines.size(); ++i) {
        const LayoutThemeWrite& b = asked.baselines[i];
        const std::string at = "baseline " + std::to_string(i) + ": ";
        std::string wrong;
        if (!place_of(b.layout)) {
            wrong = "no layout at place " + std::to_string(b.layout) + " -- this Workshop holds " +
                    std::to_string(layouts);
        } else if (b.theme.name.empty() && b.theme.fingerprint != 0) {
            wrong = "a baseline of no theme has no fingerprint";
        } else if (!b.theme.name.empty()) {
            wrong = prefs_persist::layout_theme_name_problem(b.theme.name);
        }
        if (wrong.empty() && prefs_bad_) {
            wrong = "Workshop's settings file " + host_->prefs_path +
                    " could not be read at launch and is never written over";
        }
        if (!wrong.empty()) {
            refuse(at + wrong, current);
            return;
        }
    }
    // WRITTEN TOGETHER, ON COPIES: a write a lock holds, a key nothing takes, or a pane its layout
    // does not seat is passed over and said; a bound the batch passes refuses it whole.
    SetupState next = session_.setup;
    prefs_persist::Prefs kept = prefs_;
    std::vector<SettingWriteResult> results;
    std::vector<std::string> live_moved;
    std::vector<std::string> offices_moved;
    const auto note = [](std::vector<std::string>& list, const std::string& s) {
        if (!holds(list, s)) {
            list.push_back(s);
        }
    };
    for (std::size_t i = 0; i < asked.writes.size(); ++i) {
        const SettingWrite& w = asked.writes[i];
        const auto index = static_cast<std::int64_t>(i);
        const PaneSetting candidate{w.key, w.flag, w.number, w.text};
        if (w.target.find('/') == std::string::npos) {
            const v2::PaneSettingRow* row = row_named(office_rows(w.target), w.key);
            if (prefs_persist::lock_on(kept, w.target, w.key) != nullptr) {
                results.push_back({index, kWriteLocked, locked_setting_refusal(w.key)});
                continue;
            }
            if (row == nullptr && !w.clear) {
                results.push_back({index, kWriteNotTaken,
                                   w.target + " declares no setting `" + w.key + "`"});
                continue;
            }
            std::optional<PaneSetting> value;
            if (!w.clear && !(candidate == PaneSetting{row->key, row->flag, row->number, row->text})) {
                value = candidate; // THE DEFAULT IS KEPT AS ITS ABSENCE.
            }
            const bool moved = prefs_persist::put_office_value(kept, w.target, w.key, value);
            results.push_back({index, moved ? kWriteWritten : kWriteUnchanged, std::string()});
            if (moved) {
                note(offices_moved, w.target);
            }
            continue;
        }
        const PaneRef ref = *pane_of_target(w.target);
        const std::size_t place = *place_of(w.layout);
        Setup& desk = desk_at(next, place);
        const SetupPane* seat = pane_of(desk, ref);
        if (seat == nullptr) {
            results.push_back({index, kWriteNotInLayout,
                               w.target + " is not in layout " + quoted_setup_name(desk.name)});
            continue;
        }
        if (prefs_persist::lock_on(kept, w.target, w.key) != nullptr ||
            prefs_persist::lock_on(kept, prefs_persist::layout_target(w.target, desk.name),
                                   w.key) != nullptr) {
            results.push_back({index, kWriteLocked, locked_setting_refusal(w.key)});
            continue;
        }
        const v2::PaneSettingRow* row = declared_row(ref, w.key);
        if (row == nullptr && !w.clear) {
            results.push_back({index, kWriteNotTaken,
                               w.target + " has no counted setting `" + w.key + "`"});
            continue;
        }
        const std::vector<PaneSetting> before = seat->settings;
        Written wrote = Written::ok();
        if (w.clear) {
            if (find_pane_setting(before, w.key) != nullptr) {
                wrote = author_pane_setting(desk, ref, w.key, std::nullopt, std::nullopt);
            }
        } else {
            wrote = author_pane_setting(desk, ref, w.key, candidate,
                                        row_setting_absence(narrow_setting_row(*row)));
        }
        if (!wrote.accepted) {
            refuse("write " + std::to_string(i) + ": " + wrote.refusal, current);
            return;
        }
        const bool moved = pane_of(desk, ref)->settings != before;
        results.push_back({index, moved ? kWriteWritten : kWriteUnchanged, std::string()});
        if (moved && place == session_.setup.active_at) {
            note(live_moved, w.target);
        }
    }
    bool baselines_moved = false;
    for (const LayoutThemeWrite& b : asked.baselines) {
        baselines_moved = prefs_persist::set_layout_theme(
                              kept, layout_at(next, *place_of(b.layout)).name, b.theme) ||
                          baselines_moved;
    }
    if (const std::string wrong = prefs_persist::kept_problem(kept);
        !(kept == prefs_) && !wrong.empty()) {
        refuse("the settings file would not keep what the batch writes: " + wrong, current);
        return;
    }
    // WRITTEN TO THE SETTINGS FILE FIRST: a batch the file does not hold is no batch.
    if (!(kept == prefs_)) {
        const prefs_persist::Prefs before = prefs_;
        prefs_ = std::move(kept);
        if (const std::string not_written = write_prefs();
            !not_written.empty() && !host_->prefs_path.empty()) {
            prefs_ = before;
            refuse("nothing written: Workshop's settings file was not written -- " + not_written,
                   current);
            return;
        }
    }
    session_.setup = std::move(next);
    // HANDED AND APPLIED, and the asker told what was handed.
    std::vector<std::string> handed = live_moved;
    for (const std::string& office : offices_moved) {
        if (office == kWorkshopProvider) {
            apply_workshop_settings();
        }
        handed.push_back(office);
    }
    if (!live_moved.empty() || !offices_moved.empty()) {
        hand_settings(mail);
    }
    std::size_t written = 0;
    for (const SettingWriteResult& r : results) {
        written += r.outcome == kWriteWritten ? 1u : 0u;
    }
    say((asked.cause.empty() ? std::string("settings") : asked.cause) + ": " +
            std::to_string(written) + " of " + std::to_string(results.size()) + " written" +
            (baselines_moved ? std::string(", baselines set") : std::string()),
        false);
    (void)mail.answer(SettingsWritten{true, std::string(), layout_settings_revision(),
                                      std::move(results), std::move(handed)});
    repaint(mail);
}

// WL-LOCK-04 -- agents/workshop/setting-locks.md
void WorkshopWeave::move_layout_prefs(const std::string& from, const std::string& to) {
    load_prefs();
    if (prefs_bad_ || from == to) {
        return;
    }
    // A NAME ANOTHER LAYOUT STILL HOLDS KEEPS WHAT IS KEPT UNDER IT.
    for (const Layout& l : layout_run(session_.setup)) {
        if (l.desk.name == from) {
            return;
        }
    }
    if (!prefs_persist::move_layout_entries(prefs_, from, to)) {
        return;
    }
    if (const std::string not_written = write_prefs();
        !not_written.empty() && !host_->prefs_path.empty()) {
        say("the settings kept for layout " + quoted_setup_name(from) + " were not written: " +
                not_written,
            true);
    }
}

// WL-SETTING-12 -- agents/workshop/settings.md
void WorkshopWeave::on(const PaneSettingPinRequested& asked, loom::Mail& mail) {
    load_prefs();
    const char* scope =
        prefs_persist::presentation_of(prefs_).pins_per_layout ? kPinsPerLayout : kPinsWorkshopWide;
    const auto refuse = [&mail, scope](std::string why, std::vector<std::string> pins) {
        (void)mail.answer(PaneSettingPinActed{false, std::move(why), std::move(pins), scope});
    };
    if (const std::string refused = settings_asker_refused(mail); !refused.empty()) {
        refuse("pins withheld: " + refused, {});
        return;
    }
    const PaneRef ref{asked.provider, asked.pane};
    if (asked.layout < -1 ||
        asked.layout >= static_cast<std::int64_t>(layout_count(session_.setup))) {
        refuse("no layout at place " + std::to_string(asked.layout) + " -- this Workshop holds " +
                   std::to_string(layout_count(session_.setup)),
               {});
        return;
    }
    std::vector<std::string> pins = effective_pins(ref, asked.layout);
    const RuntimePane* offered = session_.panes.runtime.find(ref.provider, ref.pane);
    const std::vector<v2::PaneSettingRow>* rows =
        offered != nullptr ? counted_settings_v2(*offered) : nullptr;
    if (rows == nullptr) {
        refuse("nothing here declares settings for " + ref_text(ref), pins);
        return;
    }
    if (prefs_bad_) {
        refuse("pins not changed -- Workshop's settings file " + host_->prefs_path +
                   " could not be read at launch and is never written over",
               pins);
        return;
    }
    bool declared = false;
    for (const v2::PaneSettingRow& row : *rows) {
        declared = declared || row.key == asked.key;
    }
    const bool pinned = holds(pins, asked.key);
    const std::string act = asked.act;
    if (act == kPinResetAct) {
        if (!asked.key.empty()) {
            refuse("`reset` names no key -- it puts the whole list back to the declared pins", pins);
            return;
        }
        std::erase_if(prefs_.pins, [&](const prefs_persist::PinList& l) {
            return l.target == pin_target(ref, asked.layout);
        });
    } else if (act == kPinAct) {
        if (!declared) {
            refuse(ref_text(ref) + " declares no setting `" + asked.key + "` to pin", pins);
            return;
        }
        if (pinned) {
            refuse("`" + asked.key + "` is pinned already", pins);
            return;
        }
        if (pins.size() >= kMaxPinsPerPane) {
            refuse("a pane pins at most " + std::to_string(kMaxPinsPerPane) +
                       " settings -- unpin one first",
                   pins);
            return;
        }
        pins.push_back(asked.key);
    } else if (act == kUnpinAct || act == kPinUpAct || act == kPinDownAct) {
        if (!pinned) {
            refuse("`" + asked.key + "` is not pinned" +
                       (act == kUnpinAct ? std::string() : std::string(" -- pin it first")),
                   pins);
            return;
        }
        const auto at = std::find(pins.begin(), pins.end(), asked.key);
        if (act == kUnpinAct) {
            pins.erase(at);
        } else if (act == kPinUpAct && at != pins.begin()) {
            std::iter_swap(at, at - 1);
        } else if (act == kPinDownAct && at + 1 != pins.end()) {
            std::iter_swap(at, at + 1);
        }
    } else {
        refuse("`" + act + "` is not a pin act -- pin, unpin, up, down or reset", pins);
        return;
    }
    if (act != kPinResetAct) {
        // THE FIRST ACT COPIES THE LIST IT READ INTO THE SCOPE'S OWN, then edits it.
        if (prefs_persist::pin_list(prefs_, pin_target(ref, asked.layout)) == nullptr &&
            prefs_.pins.size() >= prefs_persist::kMaxPinLists) {
            refuse("Workshop keeps at most " + std::to_string(prefs_persist::kMaxPinLists) +
                       " pin lists",
                   effective_pins(ref, asked.layout));
            return;
        }
        prefs_persist::put_pin_list(prefs_,
                                    prefs_persist::PinList{pin_target(ref, asked.layout), pins});
    }
    const std::string not_written = write_prefs();
    const std::vector<std::string> now = effective_pins(ref, asked.layout);
    std::string listed;
    for (const std::string& key : now) {
        listed += (listed.empty() ? "" : ", ") + key;
    }
    say("pins of " + ref_text(ref) + " (" + scope + "): " +
            (listed.empty() ? std::string("none") : listed) +
            (not_written.empty() ? std::string() : "; not written: " + not_written),
        !not_written.empty() && !host_->prefs_path.empty());
    (void)mail.answer(PaneSettingPinActed{true, std::string(), now, scope});
    repaint(mail);
}

} // namespace zengine::workshop
