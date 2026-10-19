// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// `WorkshopWeave`'s office-wide settings and pins: what an office declares it takes, one office's
// settings read and written by message, Workshop's own applied, every office handed its own, the
// settings file written only while its bytes are the ones this host read, and a pane's pin list
// edited in the list the pin scope reads.
// Workshop law: agents/workshop/settings.md

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
    if (prefs_persist::kept_count(prefs_) > kMaxOfficeSettingsKept) {
        prefs_ = before;
        refuse("Workshop keeps at most " + std::to_string(kMaxOfficeSettingsKept) +
                   " office-wide settings",
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

std::vector<std::string> WorkshopWeave::locked_keys(const PaneRef& ref) const {
    std::vector<std::string> out;
    const std::string target = pane_target(ref);
    for (const prefs_persist::SettingLock& l : prefs_.locks) {
        if (l.target == target) {
            out.push_back(l.key);
        }
    }
    return out;
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
