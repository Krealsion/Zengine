// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The layouts' store: the run and its edits, what each pane and office declares, the revisions,
// every settings door's judgement, and the files.
// Workshop law: agents/workshop/settings.md, agents/workshop/setting-locks.md,
// agents/workshop/layouts.md, agents/workshop/session-restore.md

#include "workshop/layouts/store.hpp"

#include "workshop/persist.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace zengine::workshop {

namespace {

/// What to say when there is no setup file to save to or restore from, said in one place.
constexpr const char* kNoSetupFile = "no setup file -- start Workshop with --setup <path>";

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

/// Whether two runs hold the same layouts, by id, in the same order, the same one live.
bool same_run(const SetupState& a, const SetupState& b) {
    const std::vector<Layout> x = layout_run(a);
    const std::vector<Layout> y = layout_run(b);
    if (x.size() != y.size() || a.active_at != b.active_at) {
        return false;
    }
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (!(x[i] == y[i]) || x[i].id != y[i].id) {
            return false;
        }
    }
    return true;
}

/// The desk at run position `at`, to write; out of range, the live one.
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

// ---- What one ask said and moved --------------------------------------------------------------

void LayoutStore::say(std::string said, bool bad) {
    effects_.said = std::move(said);
    effects_.bad = bad;
}

void LayoutStore::moved() {
    ++revision_;
    effects_.live_moved = true;
}

void LayoutStore::office_moved(const std::string& office) {
    if (!holds(effects_.offices, office)) {
        effects_.offices.push_back(office);
    }
}

StoreEffects LayoutStore::take_effects() {
    StoreEffects out = std::move(effects_);
    effects_ = StoreEffects{};
    return out;
}

// ---- The catalog ------------------------------------------------------------------------------

std::string LayoutStore::name_of(const PaneRef& ref) const {
    for (const CatalogName& c : catalog_) {
        if (c.ref == ref && !c.name.empty()) {
            return c.name;
        }
    }
    return ref_text(ref);
}

bool LayoutStore::presentable(const PaneRef& ref) const {
    for (const CatalogName& c : catalog_) {
        if (c.ref == ref) {
            return c.presentable;
        }
    }
    return false;
}

// ---- What each pane and office takes ----------------------------------------------------------

bool LayoutStore::counts(loom::WeaveId from, std::int64_t incarnation,
                         std::string_view office) const {
    // COUNTED WHILE THE INCARNATION THAT SENT IT HOLDS THE OFFICE: a reload in place keeps the id
    // and moves the incarnation.
    if (!from.valid() || !host_->role_holder || host_->role_holder(office) != from) {
        return false;
    }
    return !host_->incarnation_of || host_->incarnation_of(from) == incarnation;
}

// WL-SETTING-03 -- agents/workshop/settings.md
std::string LayoutStore::declare_pane(std::string_view office, const std::string& pane,
                                      std::vector<v2::PaneSettingRow> rows, loom::WeaveId from) {
    if (const Written o = check_pane_key(office, "provider"); !o.accepted) {
        return o.refusal;
    }
    if (const Written p = check_pane_key(pane, "pane key"); !p.accepted) {
        return p.refusal;
    }
    const PaneRef ref{std::string(office), pane};
    // JUDGED WHOLE BY THE DECLARATION'S OWN RULES; only an accepted one replaces what stands.
    if (const std::string wrong = v2::pane_settings_declared_problem(v2::PaneSettingsDeclared{pane, rows});
        !wrong.empty()) {
        return name_of(ref) + " @" + ref.provider + ": its settings were not taken -- " + wrong;
    }
    auto kept = std::find_if(pane_declarations_.begin(), pane_declarations_.end(),
                             [&](const PaneDeclaration& d) { return d.ref == ref; });
    if (kept == pane_declarations_.end()) {
        // A DECLARATION THAT NO LONGER COUNTS MAKES ROOM: a pane's office reloaded or gone.
        std::erase_if(pane_declarations_, [this](const PaneDeclaration& d) {
            return !counts(d.from, d.incarnation, d.ref.provider);
        });
        if (pane_declarations_.size() >= kMaxPaneCatalogEntries) {
            return name_of(ref) + " @" + ref.provider + ": its settings were not taken -- at most " +
                   std::to_string(kMaxPaneCatalogEntries) + " panes declare settings here";
        }
        pane_declarations_.push_back(PaneDeclaration{ref, {}, {}, {}, 0});
        kept = pane_declarations_.end() - 1;
    }
    kept->narrow.clear();
    for (const v2::PaneSettingRow& one : rows) {
        kept->narrow.push_back(narrow_setting_row(one));
    }
    kept->rows = std::move(rows);
    kept->from = from;
    kept->incarnation = host_->incarnation_of ? host_->incarnation_of(from) : 0;
    moved();
    return std::string();
}

// WL-SETTING-03 -- agents/workshop/settings.md
const std::vector<v2::PaneSettingRow>* LayoutStore::counted(const PaneRef& ref) const {
    for (const PaneDeclaration& d : pane_declarations_) {
        if (d.ref == ref) {
            return counts(d.from, d.incarnation, ref.provider) ? &d.rows : nullptr;
        }
    }
    return nullptr;
}

const std::vector<PaneSettingRow>* LayoutStore::counted_v1(const PaneRef& ref) const {
    for (const PaneDeclaration& d : pane_declarations_) {
        if (d.ref == ref) {
            return counts(d.from, d.incarnation, ref.provider) ? &d.narrow : nullptr;
        }
    }
    return nullptr;
}

// WL-SETTING-10 -- agents/workshop/settings.md
const std::vector<v2::PaneSettingRow>* LayoutStore::office_rows(std::string_view office) const {
    for (const OfficeDeclaration& d : office_declarations_) {
        if (d.office == office && counts(d.from, d.incarnation, d.office)) {
            return &d.rows;
        }
    }
    // WORKSHOP'S OWN ARE KNOWN HERE TOO, so its settings are judged before its declaration comes.
    static const std::vector<v2::PaneSettingRow> own = prefs_persist::workshop_office_rows();
    return office == kWorkshopProvider ? &own : nullptr;
}

// WL-SETTING-10 -- agents/workshop/settings.md
std::string LayoutStore::declare_office(const std::string& office,
                                        const std::vector<v2::PaneSettingRow>& rows,
                                        loom::WeaveId from) {
    const std::string at = "office-wide settings of " + office + ": ";
    if (office == kWorkshopProvider) {
        // WORKSHOP'S OWN ARE THIS BUILD'S, `workshop.` keys and all: its declaration names them.
        if (rows != prefs_persist::workshop_office_rows()) {
            return at + "refused -- Workshop declares the settings this build keeps for it";
        }
    } else if (const std::string wrong = v2::pane_settings_declared_problem(
                   v2::PaneSettingsDeclared{std::string(), rows});
               !wrong.empty()) {
        return at + "refused -- " + wrong;
    }
    auto kept = std::find_if(office_declarations_.begin(), office_declarations_.end(),
                             [&](const OfficeDeclaration& d) { return d.office == office; });
    if (kept == office_declarations_.end()) {
        // ONE PLACE IS WORKSHOP'S OWN, declared or not, so the bound counts it.
        const bool workshop = std::any_of(
            office_declarations_.begin(), office_declarations_.end(),
            [](const OfficeDeclaration& d) { return d.office == kWorkshopProvider; });
        if (office_declarations_.size() + (workshop || office == kWorkshopProvider ? 1 : 2) >
            kMaxSettingOffices) {
            return at + "refused -- at most " + std::to_string(kMaxSettingOffices) +
                   " offices declare office-wide settings, Workshop's own among them";
        }
        office_declarations_.push_back(OfficeDeclaration{office, {}, {}, 0, 0});
        kept = office_declarations_.end() - 1;
    }
    kept->rows = rows;
    kept->from = from;
    kept->incarnation = host_->incarnation_of ? host_->incarnation_of(from) : 0;
    kept->handed = 0;
    office_moved(office);
    return std::string();
}

// ---- Revisions --------------------------------------------------------------------------------

// WL-SETTING-07 -- agents/workshop/settings.md
std::int64_t LayoutStore::row_revision(const PaneRef& ref) {
    // A ROW THE LIVE LAYOUT NO LONGER HAS TAKES ITS RECORD WITH IT, so the list is bounded by the
    // desk's rows, and the row coming back is minted afresh.
    const Setup& live = live_desk(run_);
    std::erase_if(settings_revisions_, [&live](const SettingsRevision& r) {
        return pane_of(live, r.ref) == nullptr;
    });
    const SetupPane* seat = pane_of(live, ref);
    if (seat == nullptr) {
        return 0;
    }
    const std::vector<v2::PaneSettingRow>* rows = counted(ref);
    // THE HOLDER IS NOT MINTED IN: a new holder is handed the row at the revision it stands at, and
    // the presenter keeps a word from any other weave off the row.
    SettingsRevision now{ref, 0, run_.put_live, seat->settings,
                         rows != nullptr ? *rows : std::vector<v2::PaneSettingRow>{}};
    for (SettingsRevision& kept : settings_revisions_) {
        if (!(kept.ref == ref)) {
            continue;
        }
        if (kept.desk != now.desk || kept.settings != now.settings || kept.rows != now.rows) {
            now.revision = ++settings_revision_;
            kept = std::move(now);
        }
        return kept.revision;
    }
    now.revision = ++settings_revision_;
    settings_revisions_.push_back(std::move(now));
    return settings_revisions_.back().revision;
}

// WL-SETTING-10 -- agents/workshop/settings.md
std::int64_t LayoutStore::office_revision(const std::string& office) {
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
std::vector<std::pair<std::string, OfficeSettings>> LayoutStore::office_hand_offs() {
    std::vector<std::pair<std::string, OfficeSettings>> out;
    if (!host_->holder_accepts || !host_->role_holder) {
        return out;
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
        out.emplace_back(d.office,
                         OfficeSettings{revision, kept != nullptr ? kept->settings
                                                                  : std::vector<PaneSetting>{}});
    }
    return out;
}

void LayoutStore::office_handed(const std::string& office, std::int64_t revision) {
    for (OfficeDeclaration& d : office_declarations_) {
        if (d.office == office) {
            d.handed = revision;
        }
    }
}

// ---- A row's settings, handed to its pane -----------------------------------------------------

const LayoutStore::RowHanded* LayoutStore::handed_of(const PaneRef& ref) const {
    for (const RowHanded& h : handed_) {
        if (h.ref == ref) {
            return &h;
        }
    }
    return nullptr;
}

std::optional<std::pair<loom::WeaveId, bool>> LayoutStore::taker_of(const PaneRef& ref) const {
    if (!host_->holder_accepts || !host_->role_holder || ref.provider == kWorkshopProvider) {
        return std::nullopt;
    }
    const loom::WeaveId holder = host_->role_holder(ref.provider);
    if (!holder.valid()) {
        return std::nullopt;
    }
    // THE NEWEST VERSION IT TAKES, whether or not, and whenever, it declared.
    if (host_->holder_accepts(ref.provider, *loom::schema_of<v2::PaneSettings>())) {
        return std::make_pair(holder, true);
    }
    if (host_->holder_accepts(ref.provider, *loom::schema_of<PaneSettings>())) {
        return std::make_pair(holder, false);
    }
    return std::nullopt;
}

// WL-SETTING-04 -- agents/workshop/settings.md
std::vector<PaneHandOff> LayoutStore::pane_hand_offs() {
    // WHAT A HOLDER WAS HANDED STAYS KNOWN WHILE ANY LAYOUT OF THE RUN SEATS THE ROW, so a switch
    // away and back hands only what differs; a row no layout seats takes its record with it.
    const std::vector<Layout> run = layout_run(run_);
    std::erase_if(handed_, [&run](const RowHanded& h) {
        return std::none_of(run.begin(), run.end(),
                            [&h](const Layout& l) { return pane_of(l.desk, h.ref) != nullptr; });
    });
    std::vector<PaneHandOff> out;
    for (const SetupPane& row : run_.active.panes) {
        const std::optional<std::pair<loom::WeaveId, bool>> taker = taker_of(row.ref);
        if (!taker || !presentable(row.ref)) {
            continue; // a holder without the door, or a pane no holder offers, is handed nothing
        }
        // A v2 HOLDER IS HANDED THE ROW'S REVISION TOO, so it is handed again whenever the revision
        // moves, a desk put live with the same settings included.
        const std::int64_t revision = taker->second ? row_revision(row.ref) : 0;
        std::vector<PaneSetting> held = pane_held_settings(row.settings);
        const RowHanded* was = handed_of(row.ref);
        if (was != nullptr && was->holder == taker->first && was->revision == revision &&
            was->settings == held) {
            continue; // what it holds is what this layout keeps: saying so again is noise
        }
        out.push_back(PaneHandOff{row.ref, taker->first, taker->second,
                                  v2::PaneSettings{row.ref.pane, revision, std::move(held)}});
    }
    // A PANE A WINDOW SHOWS WHILE THE LIVE LAYOUT SEATS IT NOWHERE has no row: what it asked for is
    // every setting at its default, and it is handed that once.
    for (const PaneRef& ref : asked_unseated_) {
        const std::optional<std::pair<loom::WeaveId, bool>> taker = taker_of(ref);
        if (taker && pane_of(run_.active, ref) == nullptr) {
            out.push_back(PaneHandOff{ref, taker->first, taker->second,
                                      v2::PaneSettings{ref.pane, 0, {}}});
        }
    }
    asked_unseated_.clear();
    return out;
}

void LayoutStore::pane_handed(const PaneHandOff& handed) {
    RowHanded now{handed.ref, handed.holder, handed.value.revision, handed.value.settings, {}};
    for (RowHanded& h : handed_) {
        if (h.ref == handed.ref) {
            // WHAT THE PANE SAID IT COULD NOT USE WAS ABOUT WHAT IT NO LONGER HOLDS.
            effects_.unused_moved = effects_.unused_moved || !h.unused.empty();
            h = std::move(now);
            return;
        }
    }
    handed_.push_back(std::move(now));
}

void LayoutStore::pane_asked(const PaneRef& ref) {
    const auto at = std::find_if(handed_.begin(), handed_.end(),
                                 [&ref](const RowHanded& h) { return h.ref == ref; });
    if (at != handed_.end()) {
        effects_.unused_moved = effects_.unused_moved || !at->unused.empty();
        handed_.erase(at);
    }
    if (pane_of(run_.active, ref) == nullptr &&
        std::find(asked_unseated_.begin(), asked_unseated_.end(), ref) == asked_unseated_.end() &&
        asked_unseated_.size() < kMaxSetupPanes) {
        asked_unseated_.push_back(ref);
    }
}

// WL-SETTING-08 -- agents/workshop/settings.md
void LayoutStore::keep_unused(std::string_view office, loom::WeaveId from,
                              const PaneSettingUnused& unused) {
    const PaneRef ref{std::string(office), unused.pane};
    const auto at = std::find_if(handed_.begin(), handed_.end(),
                                 [&ref](const RowHanded& h) { return h.ref == ref; });
    // FROM THE HOLDER THE ROW WAS HANDED TO, and from no other: a revision is the row's, so a word
    // from the office's former holder would otherwise land on its successor's.
    if (at == handed_.end() || from != at->holder) {
        return;
    }
    // ABOUT WHAT IT HOLDS NOW, or about nothing: a word naming a revision it was not last handed
    // is late, and is dropped as the revision it names was.
    if (unused.revision <= 0 || unused.revision != at->revision ||
        pane_of(live_desk(run_), ref) == nullptr || row_revision(ref) != at->revision) {
        return;
    }
    const std::string said_at = name_of(ref) + " @" + ref.provider + ": ";
    std::string wrong = pane_setting_key_problem(unused.key);
    if (wrong.empty() && (unused.why.empty() || !pane_setting_text_ok(unused.why))) {
        wrong = "why a setting is unused is 1 to " + std::to_string(kMaxPaneSettingTextLen) +
                " bytes of printable ASCII";
    }
    if (!wrong.empty()) {
        say(said_at + "a setting it cannot use was not kept -- " + wrong, true);
        return;
    }
    for (RowSettingUnused& kept : at->unused) {
        if (kept.key == unused.key) {
            if (kept.why != unused.why) {
                kept.why = unused.why;
                effects_.unused_moved = true;
            }
            return; // said again: nothing new to tell
        }
    }
    if (at->unused.size() >= kMaxPaneSettingsPerRow) {
        say(said_at + "a setting it cannot use was not kept -- a pane keeps at most " +
                std::to_string(kMaxPaneSettingsPerRow) + " of them",
            true);
        return;
    }
    at->unused.push_back(RowSettingUnused{ref.provider, ref.pane, unused.key, unused.why});
    effects_.unused_moved = true;
}

std::vector<RowSettingUnused> LayoutStore::unused_of(const PaneRef& ref) const {
    const RowHanded* h = handed_of(ref);
    return h != nullptr ? h->unused : std::vector<RowSettingUnused>{};
}

// WL-SETTING-05 -- agents/workshop/settings.md
std::string LayoutStore::handed_fact(const PaneRef& ref) {
    if (ref.provider == kWorkshopProvider) {
        return "stored; not handed: Workshop's own pane takes no settings";
    }
    const SetupPane* seat = pane_of(live_desk(run_), ref);
    if (seat == nullptr) {
        return "stored; not handed: the live layout has no row for " + ref_text(ref);
    }
    if (!host_->role_holder || !host_->role_holder(ref.provider).valid()) {
        return "stored; not handed: nothing holds `" + ref.provider + "` now";
    }
    if (!presentable(ref)) {
        return "stored; not handed: nothing offers " + ref_text(ref) + " here";
    }
    const std::optional<std::pair<loom::WeaveId, bool>> taker = taker_of(ref);
    if (!taker) {
        return "stored; not handed: " + ref.provider + " takes no settings";
    }
    // HANDED AT THE ROW'S REVISION, when the holder is handed one: settings equal to an earlier
    // revision's are not what this row was last minted for.
    const RowHanded* was = handed_of(ref);
    const std::int64_t revision = taker->second ? row_revision(ref) : 0;
    if (was == nullptr || was->holder != taker->first || was->revision != revision ||
        was->settings != pane_held_settings(seat->settings)) {
        return "stored; not handed: the hand-off was not queued";
    }
    // ...AND WHAT THE PANE SAID IT CANNOT USE OF WHAT IT WAS HANDED, in its own words.
    std::string said = "stored and handed";
    for (const RowSettingUnused& u : was->unused) {
        said += "; it cannot use " + u.key + " -- " + u.why;
    }
    return said;
}

// ---- The live layout, for its presenter -------------------------------------------------------

LiveLayout LayoutStore::live_layout() {
    LiveLayout out;
    out.revision = revision_;
    out.put_live = static_cast<std::int64_t>(run_.put_live);
    const std::size_t at = live_at(run_);
    out.id = run_.active_id;
    out.position = static_cast<std::int64_t>(at);
    // THE PRESENTER'S COPY HOLDS WORKSHOP'S OWN WORDS ON EACH ROW and nothing of the pane's: what a
    // pane's settings mean is the pane's, and the owner hands them to it.
    Setup presented = run_.active;
    for (SetupPane& row : presented.panes) {
        std::erase_if(row.settings,
                      [](const PaneSetting& s) { return !is_desk_setting_key(s.key); });
    }
    out.desk = setup_persist::to_setup(presented);
    out.link_path = run_.active_link.path;
    for (const Layout& l : layout_run(run_)) {
        out.tabs.push_back(LiveTab{l.id, l.desk.name, link_status(l.desk, l.link)});
    }
    for (const SetupPane& row : run_.active.panes) {
        const std::vector<RowSettingUnused> unused = unused_of(row.ref);
        out.unused.insert(out.unused.end(), unused.begin(), unused.end());
    }
    out.session_refused = session_refusal_;
    out.settings_refused = prefs_refusal_;
    return out;
}

void LayoutStore::put_run(SetupState run) {
    run_ = std::move(run);
    settle_live();
    moved();
}

// ---- The settings file ------------------------------------------------------------------------

// WL-SETTING-11 -- agents/workshop/settings.md
void LayoutStore::load_prefs() {
    if (prefs_loaded_) {
        return;
    }
    prefs_loaded_ = true;
    std::error_code ec;
    if (host_->prefs_path.empty() || !std::filesystem::exists(host_->prefs_path, ec)) {
        return;
    }
    // READ ONCE, AND THE BYTES KEPT: the baseline a write compares the file against, so a change
    // by another hand since this read is seen rather than overwritten.
    const persist::FileText read = persist::read_file(
        host_->prefs_path, prefs_persist::kMaxPrefsBytes, "a Workshop prefs file");
    if (read.outcome.accepted) {
        prefs_bytes_ = read.text;
        prefs_file_present_ = true;
    }
    const prefs_persist::LoadedPrefs loaded =
        read.outcome.accepted ? prefs_persist::from_text(read.text)
                              : prefs_persist::LoadedPrefs{read.outcome, {}, {}};
    if (!loaded.outcome.accepted) {
        // EVERY LATER WRITE IS REFUSED, and the refusal is held all run.
        prefs_bad_ = true;
        prefs_refusal_ = loaded.outcome.refusal;
        return;
    }
    prefs_ = loaded.kept;
    office_moved(kWorkshopProvider);
}

// WL-SETTING-11 -- agents/workshop/settings.md
std::string LayoutStore::write_prefs() {
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
    // WHAT THE FILE KEPT FOR LAYOUTS IS CARRIED UNCHANGED until a session holds it, then dropped.
    const prefs_persist::Prefs kept =
        layout_entries_held_ ? prefs_persist::without_layout_entries(prefs_) : prefs_;
    const std::string text = prefs_persist::to_text(kept);
    const Written wrote = persist::write_file_making_room(host_->prefs_path, text);
    if (!wrote.accepted) {
        return wrote.refusal;
    }
    prefs_ = kept;
    prefs_bytes_ = text;
    prefs_file_present_ = true;
    return std::string();
}

bool LayoutStore::carries_layout_entries() const {
    return prefs_persist::has_layout_entries(prefs_);
}

// ---- Pins and locks ---------------------------------------------------------------------------

// WL-SESSION-05 -- agents/workshop/session-restore.md
std::optional<std::size_t> LayoutStore::place_of(std::int64_t layout) const {
    return position_of(run_, layout);
}

std::string LayoutStore::gone(std::int64_t layout) const {
    return "that layout is gone -- no layout of this Workshop holds id " + std::to_string(layout);
}

// WL-SETTING-12 -- agents/workshop/settings.md
std::vector<std::string> LayoutStore::effective_pins(const PaneRef& ref, std::size_t at) const {
    // THE SCOPE'S OWN LIST -- per layout, the layout's own -- ELSE THE WORKSHOP-WIDE ONE, ELSE THE
    // DECLARED PINS: a reading falls back, and no list is written by reading it.
    if (prefs_persist::presentation_of(prefs_).pins_per_layout) {
        for (const SetupPins& own : layout_at(run_, at).pins) {
            if (own.ref == ref) {
                return own.keys;
            }
        }
    }
    if (const prefs_persist::PinList* wide = prefs_persist::pin_list(prefs_, pane_target(ref))) {
        return wide->keys;
    }
    std::vector<std::string> declared;
    if (const std::vector<v2::PaneSettingRow>* rows = counted(ref)) {
        for (const v2::PaneSettingRow& row : *rows) {
            if (row.pinned) {
                declared.push_back(row.key);
            }
        }
    }
    return declared;
}

// WL-LOCK-01 -- agents/workshop/setting-locks.md
std::vector<SettingLockShown> LayoutStore::locks_on(const PaneRef& ref, std::size_t at) const {
    std::vector<SettingLockShown> out;
    // EVERY EVERYWHERE LOCK ON THE PANE, AND THE LAYOUT'S OWN on a key no everywhere lock holds.
    const std::string target = pane_target(ref);
    for (const SettingLock& l : prefs_.locks) {
        if (l.target == target) {
            out.push_back(SettingLockShown{l.key, kLockScopeEverywhere});
        }
    }
    for (const SetupLock& l : layout_at(run_, at).locks) {
        if (l.ref == ref && prefs_persist::lock_on(prefs_, target, l.key) == nullptr) {
            out.push_back(SettingLockShown{l.key, kLockScopeLayout});
        }
    }
    std::sort(out.begin(), out.end(), [](const SettingLockShown& a, const SettingLockShown& b) {
        return a.key < b.key;
    });
    return out;
}

std::vector<std::string> LayoutStore::locked_keys(const PaneRef& ref) const {
    std::vector<std::string> out;
    for (const SettingLockShown& l : locks_on(ref, live_at(run_))) {
        out.push_back(l.key);
    }
    return out;
}

// WL-LOCK-01 -- agents/workshop/setting-locks.md
bool LayoutStore::pin_everywhere_locks(Setup& desk) const {
    bool any = false;
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
        any = any || pane_of(desk, *ref)->settings != before;
    }
    return any;
}

// A ROW THE LIVE DESK SEATS WEARS EVERY EVERYWHERE LOCK ON ITS PANE, one seated since included.
void LayoutStore::settle_live() { (void)pin_everywhere_locks(live_desk(run_)); }

const v2::PaneSettingRow* LayoutStore::declared_row(const PaneRef& ref,
                                                    const std::string& key) const {
    static const v2::PaneSettingRow skin_row = workshop_skin_setting_row();
    if (key == kWorkshopSkinKey) {
        return &skin_row;
    }
    return row_named(counted(ref), key);
}

// ---- One pane's row of the live layout --------------------------------------------------------

// WL-SETTING-07 -- agents/workshop/settings.md
PaneSettingsAnswered LayoutStore::read_pane(const std::string& refused,
                                         const PaneSettingsRequested& asked) {
    PaneSettingsAnswered shown;
    shown.provider = asked.provider;
    shown.pane = asked.pane;
    if (!refused.empty()) {
        shown.said = "settings withheld: " + refused;
        return shown;
    }
    const PaneRef ref{asked.provider, asked.pane};
    load_prefs();
    if (const std::vector<v2::PaneSettingRow>* rows = counted(ref)) {
        shown.rows = *rows;
    }
    shown.pins = effective_pins(ref, live_at(run_));
    shown.locks = locks_on(ref, live_at(run_));
    const SetupPane* seat = pane_of(live_desk(run_), ref);
    if (seat == nullptr) {
        shown.said = ref_text(ref) + " has no row on the live layout";
        return shown;
    }
    shown.revision = row_revision(ref);
    shown.settings = seat->settings;
    // HOW FAR THE ROW'S HAND-OFF GOT, beside what it keeps: a pane's fact, and Workshop's own panes
    // are handed nothing to say it of.
    if (ref.provider != kWorkshopProvider) {
        shown.said = handed_fact(ref);
    }
    return shown;
}

// WL-SETTING-07 -- agents/workshop/settings.md
PaneSettingCommitAnswered LayoutStore::commit_pane(const std::string& refused,
                                          const PaneSettingCommitRequested& asked) {
    const auto refuse = [](std::string why, std::int64_t revision) {
        return PaneSettingCommitAnswered{false, std::move(why), revision, std::string()};
    };
    // IN ORDER, EACH REFUSAL WRITING NOTHING: the asker, the row, the revision, then the value.
    if (!refused.empty()) {
        return refuse("settings commit withheld: " + refused, 0);
    }
    const PaneRef ref{asked.provider, asked.pane};
    Setup& live = live_desk(run_);
    if (pane_of(live, ref) == nullptr) {
        return refuse(ref_text(ref) + " has no row on the live layout -- open it first", 0);
    }
    const std::int64_t current = row_revision(ref);
    if (asked.revision != current) {
        return refuse("stale revision " + std::to_string(asked.revision) + " -- the row of " +
                          ref_text(ref) + " stands at " + std::to_string(current) +
                          "; read it again",
                      current);
    }
    const std::string name = name_of(ref);
    const std::vector<PaneSetting> before = pane_of(live, ref)->settings;
    const int fields = (asked.flag ? 1 : 0) + (asked.number ? 1 : 0) + (asked.text ? 1 : 0);
    // A LOCK HOLDS ITS KEY against every write, a clear included.
    load_prefs();
    if (holds(locked_keys(ref), asked.key)) {
        return refuse(locked_setting_refusal(asked.key), current);
    }
    std::string written;
    if (asked.clear) {
        // A CLEAR NAMES A KEY ALONE, declared or kept, and clearing an absent key changes nothing.
        if (fields != 0) {
            return refuse("a clear takes no value -- name the key alone", current);
        }
        if (find_pane_setting(before, asked.key) != nullptr) {
            const Written w = author_pane_setting(live, ref, asked.key, std::nullopt, std::nullopt);
            if (!w.accepted) {
                return refuse(w.refusal, current);
            }
        }
        written = "cleared " + asked.key + " of " + name;
    } else {
        const std::vector<PaneSettingRow>* rows = counted_v1(ref);
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
            return refuse(ref_text(ref) + " has no counted setting `" + asked.key +
                              "` -- a key no declaration takes is only cleared",
                          current);
        }
        const PaneSetting candidate{asked.key, asked.flag, asked.number, asked.text};
        if (const std::string wrong = pane_setting_refused_by(*declared, candidate);
            !wrong.empty()) {
            return refuse(wrong, current);
        }
        const Written w =
            author_pane_setting(live, ref, asked.key, candidate, row_setting_absence(*declared));
        if (!w.accepted) {
            return refuse(w.refusal, current);
        }
        written = "committed " + asked.key + " of " + name + " = " + setting_value_text(candidate);
    }
    if (pane_of(live, ref)->settings == before) {
        // NOTHING MOVED, so nothing is minted or handed: the asker's revision still stands.
        return PaneSettingCommitAnswered{true, std::string(), current, std::string()};
    }
    moved();
    say(written, false);
    // ...SAID WITH HOW FAR THE ROW'S HAND-OFF GOT, once its holder was handed it.
    effects_.said_provider = ref.provider;
    effects_.said_pane = ref.pane;
    return PaneSettingCommitAnswered{true, std::string(), row_revision(ref), "stored"};
}

// ---- Office-wide settings ---------------------------------------------------------------------

// WL-SETTING-10 -- agents/workshop/settings.md
OfficeSettingsAnswered LayoutStore::read_office(const std::string& refused,
                                             const OfficeSettingsRequested& asked) {
    load_prefs();
    OfficeSettingsAnswered shown;
    shown.office = asked.office;
    if (!refused.empty()) {
        shown.written = "settings withheld: " + refused;
        return shown;
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
    return shown;
}

// WL-SETTING-10 -- agents/workshop/settings.md
OfficeSettingCommitAnswered LayoutStore::commit_office(const std::string& refused,
                                            const OfficeSettingCommitRequested& asked) {
    load_prefs();
    const auto refuse = [](std::string why, std::int64_t revision) {
        return OfficeSettingCommitAnswered{false, std::move(why), revision, std::string()};
    };
    // IN ORDER, EACH REFUSAL WRITING NOTHING: the asker, the office, the revision, the file, the
    // key, the lock, then the value.
    if (!refused.empty()) {
        return refuse("settings commit withheld: " + refused, 0);
    }
    const std::vector<v2::PaneSettingRow>* rows = office_rows(asked.office);
    if (rows == nullptr) {
        return refuse("nothing declares office-wide settings for `" + asked.office + "` here", 0);
    }
    const std::int64_t current = office_revision(asked.office);
    if (asked.revision != current) {
        return refuse("stale revision " + std::to_string(asked.revision) + " -- the settings of " +
                          asked.office + " stand at " + std::to_string(current) +
                          "; read them again",
                      current);
    }
    if (prefs_bad_) {
        return refuse("Workshop's settings file " + host_->prefs_path +
                          " could not be read at launch and is never written over -- mend or "
                          "remove it, then relaunch",
                      current);
    }
    const v2::PaneSettingRow* declared = row_named(rows, asked.key);
    const int fields = (asked.flag ? 1 : 0) + (asked.number ? 1 : 0) + (asked.text ? 1 : 0);
    if (asked.clear && fields != 0) {
        return refuse("a clear takes no value -- name the key alone", current);
    }
    if (declared == nullptr &&
        !(asked.clear && prefs_persist::office_value(prefs_, asked.office, asked.key) != nullptr)) {
        return refuse(asked.office + " declares no setting `" + asked.key +
                          "` -- a key no declaration takes is only cleared",
                      current);
    }
    if (prefs_persist::lock_on(prefs_, asked.office, asked.key) != nullptr) {
        return refuse(locked_setting_refusal(asked.key), current);
    }
    std::optional<PaneSetting> value;
    if (!asked.clear) {
        const PaneSetting candidate{asked.key, asked.flag, asked.number, asked.text};
        if (const std::string wrong = v2::pane_setting_refused_by(*declared, candidate);
            !wrong.empty()) {
            return refuse(wrong, current);
        }
        // THE DEFAULT IS KEPT AS ITS ABSENCE.
        if (!(candidate ==
              PaneSetting{declared->key, declared->flag, declared->number, declared->text})) {
            value = candidate;
        }
    }
    const prefs_persist::Prefs before = prefs_;
    (void)prefs_persist::put_office_value(prefs_, asked.office, asked.key, value);
    // WITHIN THE FILE'S BOUNDS, so the file written is one the next launch reads.
    if (prefs_persist::kept_count(prefs_) > kMaxOfficeSettingsKept ||
        prefs_persist::office_count(prefs_) > kMaxSettingOffices) {
        prefs_ = before;
        return refuse("Workshop keeps at most " + std::to_string(kMaxOfficeSettingsKept) +
                          " office-wide settings, for at most " +
                          std::to_string(kMaxSettingOffices) + " offices",
                      current);
    }
    if (prefs_ == before) {
        // NOTHING MOVED, so nothing is minted, written or handed.
        return OfficeSettingCommitAnswered{true, std::string(), current, "unchanged"};
    }
    // HANDED OR APPLIED BY THE HOLDER, THEN WRITTEN, AND THE TWO SAID APART.
    office_moved(asked.office);
    std::string applied;
    if (asked.office == kWorkshopProvider) {
        applied = "applied";
    } else {
        applied = host_->holder_accepts &&
                          host_->holder_accepts(asked.office, *loom::schema_of<OfficeSettings>())
                      ? "handed"
                      : "kept; not handed: " + asked.office + " takes no office-wide settings";
    }
    const std::string not_written = write_prefs();
    const std::string handed =
        applied + (not_written.empty() ? "; written" : "; not written: " + not_written);
    say((asked.clear ? "cleared " + asked.key
                     : "committed " + asked.key + " = " +
                           setting_value_text(
                               PaneSetting{asked.key, asked.flag, asked.number, asked.text})) +
            " of " + asked.office + "; " + handed,
        !not_written.empty() && !host_->prefs_path.empty());
    return OfficeSettingCommitAnswered{true, std::string(), office_revision(asked.office), handed};
}

// ---- Pins -------------------------------------------------------------------------------------

// WL-SETTING-12 -- agents/workshop/settings.md
PaneSettingPinAnswered LayoutStore::pin(const std::string& refused,
                                     const PaneSettingPinRequested& asked) {
    load_prefs();
    const char* scope =
        prefs_persist::presentation_of(prefs_).pins_per_layout ? kPinsPerLayout : kPinsWorkshopWide;
    const auto refuse = [scope](std::string why, std::vector<std::string> pins) {
        return PaneSettingPinAnswered{false, std::move(why), std::move(pins), scope};
    };
    if (!refused.empty()) {
        return refuse("pins withheld: " + refused, {});
    }
    const PaneRef ref{asked.provider, asked.pane};
    const std::optional<std::size_t> at = place_of(asked.layout);
    if (!at) {
        return refuse(gone(asked.layout), {});
    }
    std::vector<std::string> pins = effective_pins(ref, *at);
    const std::vector<v2::PaneSettingRow>* rows = counted(ref);
    if (rows == nullptr) {
        return refuse("nothing here declares settings for " + ref_text(ref), pins);
    }
    // PER LAYOUT, THE LIST IS THE LAYOUT'S OWN, kept with its desk; WORKSHOP-WIDE, the file's.
    const bool per_layout = prefs_persist::presentation_of(prefs_).pins_per_layout;
    if (!per_layout && prefs_bad_) {
        return refuse("pins not changed -- Workshop's settings file " + host_->prefs_path +
                          " could not be read at launch and is never written over",
                      pins);
    }
    const bool declared = row_named(rows, asked.key) != nullptr;
    const bool pinned = holds(pins, asked.key);
    const std::string act = asked.act;
    if (act == kPinResetAct) {
        if (!asked.key.empty()) {
            return refuse("`reset` names no key -- it puts the whole list back to the declared "
                          "pins",
                          pins);
        }
        if (per_layout) {
            std::erase_if(desk_at(run_, *at).pins,
                          [&](const SetupPins& l) { return l.ref == ref; });
        } else {
            std::erase_if(prefs_.pins, [&](const prefs_persist::PinList& l) {
                return l.target == pane_target(ref);
            });
        }
    } else if (act == kPinAct) {
        if (!declared) {
            return refuse(ref_text(ref) + " declares no setting `" + asked.key + "` to pin", pins);
        }
        if (pinned) {
            return refuse("`" + asked.key + "` is pinned already", pins);
        }
        if (pins.size() >= kMaxPinsPerPane) {
            return refuse("a pane pins at most " + std::to_string(kMaxPinsPerPane) +
                              " settings -- unpin one first",
                          pins);
        }
        pins.push_back(asked.key);
    } else if (act == kUnpinAct || act == kPinUpAct || act == kPinDownAct) {
        if (!pinned) {
            return refuse("`" + asked.key + "` is not pinned" +
                              (act == kUnpinAct ? std::string() : std::string(" -- pin it first")),
                          pins);
        }
        const auto here = std::find(pins.begin(), pins.end(), asked.key);
        if (act == kUnpinAct) {
            pins.erase(here);
        } else if (act == kPinUpAct && here != pins.begin()) {
            std::iter_swap(here, here - 1);
        } else if (act == kPinDownAct && here + 1 != pins.end()) {
            std::iter_swap(here, here + 1);
        }
    } else {
        return refuse("`" + act + "` is not a pin act -- pin, unpin, up, down or reset", pins);
    }
    if (act != kPinResetAct && per_layout) {
        // THE FIRST ACT COPIES THE LIST IT READ INTO THE LAYOUT'S OWN, then edits it -- a row the
        // layout seats, within what its desk keeps.
        Setup candidate = layout_at(run_, *at);
        if (pane_of(candidate, ref) == nullptr) {
            return refuse(ref_text(ref) + " is not in layout " + quoted_setup_name(candidate.name) +
                              " -- a layout's pins are for a row it seats",
                          effective_pins(ref, *at));
        }
        std::erase_if(candidate.pins, [&](const SetupPins& l) { return l.ref == ref; });
        candidate.pins.push_back(SetupPins{ref, pins});
        if (const Written whole = check_setup(candidate); !whole.accepted) {
            return refuse(whole.refusal, effective_pins(ref, *at));
        }
        desk_at(run_, *at) = std::move(candidate);
    } else if (act != kPinResetAct) {
        // THE FIRST ACT COPIES THE LIST IT READ INTO THE WORKSHOP-WIDE ONE, then edits it.
        if (prefs_persist::pin_list(prefs_, pane_target(ref)) == nullptr &&
            prefs_.pins.size() >= prefs_persist::kMaxPinLists) {
            return refuse("Workshop keeps at most " + std::to_string(prefs_persist::kMaxPinLists) +
                              " pin lists",
                          effective_pins(ref, *at));
        }
        prefs_persist::put_pin_list(prefs_, prefs_persist::PinList{pane_target(ref), pins});
    }
    // A LAYOUT'S OWN LIST IS KEPT WITH ITS DESK, in the session; THE WORKSHOP-WIDE ONE IS WRITTEN.
    const std::string not_written = per_layout ? std::string() : write_prefs();
    if (per_layout) {
        moved();
    }
    const std::vector<std::string> now = effective_pins(ref, *at);
    std::string listed;
    for (const std::string& key : now) {
        listed += (listed.empty() ? "" : ", ") + key;
    }
    say("pins of " + ref_text(ref) + " (" + scope + "): " +
            (listed.empty() ? std::string("none") : listed) +
            (not_written.empty() ? std::string() : "; not written: " + not_written),
        !not_written.empty() && !host_->prefs_path.empty());
    return PaneSettingPinAnswered{true, std::string(), now, scope};
}

// ---- Locks ------------------------------------------------------------------------------------

// WL-LOCK-01, WL-LOCK-02 -- agents/workshop/setting-locks.md
PaneSettingLockAnswered LayoutStore::lock(const std::string& refused,
                                       const PaneSettingLockRequested& asked, bool weavers_hand) {
    load_prefs();
    const auto refuse = [](std::string why) {
        return PaneSettingLockAnswered{false, std::move(why), {}};
    };
    // IN ORDER, EACH REFUSAL CHANGING NOTHING: the asker, the act, where it holds, the unlocking
    // hand, the key and the target, the layout, the file, then the act on the target.
    if (!refused.empty()) {
        return refuse("lock withheld: " + refused);
    }
    const bool locking = asked.act == kLockAct;
    if (!locking && asked.act != kUnlockAct) {
        return refuse("`" + asked.act + "` is not a lock act -- lock or unlock");
    }
    const bool everywhere = asked.scope == kLockScopeEverywhere;
    if (!everywhere && asked.scope != kLockScopeLayout) {
        return refuse("`" + asked.scope + "` is not where a lock holds -- layout or everywhere");
    }
    if (!locking && !weavers_hand) {
        // AN UNLOCK IS THE WEAVER'S HAND: the Terminal this host mounted, which answers the weaver
        // alone, or a pane naming the weaver's own current act.
        return refuse("unlocking is the weaver's -- from the Terminal, or from a pane on the "
                      "weaver's own current act");
    }
    if (const std::string wrong = pane_setting_key_problem(asked.key); !wrong.empty()) {
        return refuse(wrong);
    }
    if (asked.target.find('@') != std::string::npos) {
        return refuse("a lock names its layout by `layout`, never in its target");
    }
    if (const std::string wrong = prefs_persist::target_problem(asked.target, "a lock's target");
        !wrong.empty()) {
        return refuse(wrong);
    }
    const std::optional<std::size_t> place = place_of(asked.layout);
    if (!place) {
        return refuse(gone(asked.layout));
    }
    // A LAYOUT'S OWN LOCK IS KEPT WITH ITS DESK; WHAT HOLDS EVERYWHERE, BY THE SETTINGS FILE.
    if (everywhere && prefs_bad_) {
        return refuse("locks not changed -- Workshop's settings file " + host_->prefs_path +
                      " could not be read at launch and is never written over");
    }
    const std::size_t at = *place;
    const std::string& target = asked.target;
    const std::string& key = asked.key;
    const std::string layout_name = layout_at(run_, at).name;
    const std::string in_layout = "layout " + quoted_setup_name(layout_name);
    // WHAT THE ACT CHANGES STANDS ONLY ONCE THE SETTINGS FILE HOLDS IT.
    const prefs_persist::Prefs prefs_before = prefs_;
    const SetupState run_before = run_;
    std::vector<std::string> written;
    std::string done;
    if (target.find('/') == std::string::npos) {
        // AN OFFICE-WIDE SETTING HAS ONE VALUE, so a lock on it holds everywhere and pins nothing.
        const std::vector<v2::PaneSettingRow>* rows = office_rows(target);
        if (rows == nullptr) {
            return refuse("nothing declares office-wide settings for `" + target + "` here");
        }
        if (!everywhere) {
            return refuse("an office-wide setting has one value in every layout -- lock it "
                          "everywhere");
        }
        if (locking) {
            if (row_named(rows, key) == nullptr &&
                prefs_persist::office_value(prefs_, target, key) == nullptr) {
                return refuse(target + " declares no setting `" + key + "` to lock");
            }
            if (prefs_persist::lock_on(prefs_, target, key) != nullptr) {
                return refuse("`" + key + "` of " + target + " is locked already");
            }
            if (prefs_.locks.size() >= prefs_persist::kMaxLocks) {
                return refuse("Workshop keeps at most " + std::to_string(prefs_persist::kMaxLocks) +
                              " locks -- unlock one first");
            }
            prefs_persist::put_lock(prefs_, SettingLock{target, key});
            done = "locked " + key + " of " + target + " everywhere";
        } else {
            if (!prefs_persist::drop_lock(prefs_, target, key)) {
                return refuse("`" + key + "` of " + target + " is not locked");
            }
            done = "unlocked " + key + " of " + target;
        }
        office_moved(target);
    } else {
        const std::optional<PaneRef> ref = pane_of_target(target);
        if (!ref) {
            return refuse("a lock's target is `<office>/<pane>`, or `<office>` for an office-wide "
                          "setting");
        }
        const SetupLock here{*ref, key};
        const auto held_here = [&here](const Setup& desk) {
            return std::find(desk.locks.begin(), desk.locks.end(), here) != desk.locks.end();
        };
        const SetupPane* row = pane_of(layout_at(run_, at), *ref);
        const bool declared = declared_row(*ref, key) != nullptr;
        const PaneSetting* stored = row != nullptr ? find_pane_setting(row->settings, key) : nullptr;
        if (locking && prefs_persist::lock_on(prefs_, target, key) != nullptr) {
            return refuse("`" + key + "` of " + target + " is locked everywhere already");
        }
        if (locking && !declared && stored == nullptr) {
            return refuse(target + " neither declares nor keeps `" + key + "` in " + in_layout);
        }
        if (locking && everywhere && prefs_.locks.size() >= prefs_persist::kMaxLocks) {
            return refuse("Workshop keeps at most " + std::to_string(prefs_persist::kMaxLocks) +
                          " locks -- unlock one first");
        }
        if (locking && !everywhere) {
            if (row == nullptr) {
                return refuse(target + " is not in " + in_layout + " -- a layout's lock holds its row");
            }
            if (held_here(layout_at(run_, at))) {
                return refuse("`" + key + "` of " + target + " is locked in " + in_layout +
                              " already");
            }
            Setup candidate = layout_at(run_, at);
            candidate.locks.push_back(here);
            if (const Written whole = check_setup(candidate); !whole.accepted) {
                return refuse(whole.refusal);
            }
            desk_at(run_, at) = std::move(candidate);
            written = {layout_name};
            done = "locked " + key + " of " + target + " in " + in_layout;
        } else if (locking) {
            // AN EVERYWHERE LOCK PINS THE VALUE ITS LAYOUT HOLDS, its default when it keeps none,
            // and takes the place of each layout's own lock on the key.
            SettingLock lock{target, key};
            if (stored != nullptr) {
                lock.flag = stored->flag;
                lock.number = stored->number;
                lock.text = stored->text;
            } else {
                lock.clear = true;
            }
            prefs_persist::put_lock(prefs_, lock);
            // ...AND IS WRITTEN INTO EVERY ROW THAT SEATS THE PANE, each judged by its bounds, all
            // or none.
            SetupState next = run_;
            for (std::size_t i = 0; i < layout_count(next); ++i) {
                Setup& desk = desk_at(next, i);
                std::erase(desk.locks, here);
                if (pane_of(desk, *ref) == nullptr) {
                    continue;
                }
                if (const Written w = pin_lock_value(desk, *ref, lock); !w.accepted) {
                    prefs_ = prefs_before;
                    return refuse("`" + key + "` of " + target + " cannot be pinned in layout " +
                                  quoted_setup_name(desk.name) + ": " + w.refusal);
                }
                written.push_back(desk.name);
            }
            run_ = std::move(next);
            done = "locked " + key + " of " + target + " everywhere at " +
                   (lock.clear ? std::string("its default")
                               : setting_value_text(
                                     PaneSetting{key, lock.flag, lock.number, lock.text}));
        } else if (!everywhere) {
            if (!held_here(layout_at(run_, at))) {
                return refuse(prefs_persist::lock_on(prefs_, target, key) != nullptr
                                  ? "`" + key + "` of " + target +
                                        " is locked everywhere -- unlock it everywhere"
                                  : "`" + key + "` of " + target + " is not locked in " + in_layout);
            }
            std::erase(desk_at(run_, at).locks, here);
            written = {layout_name};
            done = "unlocked " + key + " of " + target + " in " + in_layout;
        } else {
            if (!prefs_persist::drop_lock(prefs_, target, key)) {
                return refuse("`" + key + "` of " + target + " is not locked everywhere");
            }
            done = "unlocked " + key + " of " + target + " everywhere";
        }
    }
    // WHAT HOLDS EVERYWHERE STANDS ONLY ONCE THE SETTINGS FILE HOLDS IT; a layout's own lock is
    // its desk's, kept in the session.
    if (!(prefs_ == prefs_before)) {
        if (const std::string not_written = write_prefs();
            !not_written.empty() && !host_->prefs_path.empty()) {
            prefs_ = prefs_before;
            run_ = run_before;
            effects_.offices.clear();
            return refuse("nothing changed: Workshop's settings file was not written -- " +
                          not_written);
        }
    }
    // A LIVE ROW AN EVERYWHERE LOCK WROTE IS HANDED NOW, as a commit's is; and every reading
    // shows the lock.
    moved();
    say(done, false);
    return PaneSettingLockAnswered{true, std::string(), std::move(written)};
}

// ---- Every layout at once ---------------------------------------------------------------------

// WL-LOCK-03 -- agents/workshop/setting-locks.md
LayoutSettingsAnswered LayoutStore::layout_settings_reading() const {
    LayoutSettingsAnswered out;
    out.active = static_cast<std::int64_t>(live_at(run_));
    const std::vector<Layout> run = layout_run(run_);
    for (std::size_t i = 0; i < run.size(); ++i) {
        LayoutSettings layout;
        layout.id = run[i].id;
        layout.position = static_cast<std::int64_t>(i);
        layout.name = run[i].desk.name;
        layout.theme = run[i].desk.theme;
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
    for (const PaneDeclaration& p : pane_declarations_) {
        const std::vector<v2::PaneSettingRow>* rows = counted(p.ref);
        if (rows == nullptr) {
            continue;
        }
        DeclaredSettings d{p.ref.provider, p.ref.pane, {workshop_skin_setting_row()}};
        for (const v2::PaneSettingRow& row : *rows) {
            if (row.key != kWorkshopSkinKey) {
                d.rows.push_back(row);
            }
        }
        out.declared.push_back(std::move(d));
    }
    out.declared.push_back(
        DeclaredSettings{kWorkshopProvider, std::string(), *office_rows(kWorkshopProvider)});
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
std::int64_t LayoutStore::layout_settings_revision() {
    LayoutSettingsAnswered now = layout_settings_reading();
    if (layout_settings_revision_ == 0 || !(now == layout_settings_minted_)) {
        layout_settings_minted_ = std::move(now);
        ++layout_settings_revision_;
    }
    return layout_settings_revision_;
}

// WL-LOCK-03 -- agents/workshop/setting-locks.md
LayoutSettingsAnswered LayoutStore::read_layouts(const std::string& refused) {
    load_prefs();
    if (!refused.empty()) {
        return LayoutSettingsAnswered{}; // nothing read, at the revision none stands at
    }
    const std::int64_t revision = layout_settings_revision();
    LayoutSettingsAnswered shown = layout_settings_minted_;
    shown.revision = revision;
    return shown;
}

// WL-LOCK-03 -- agents/workshop/setting-locks.md
SettingsWriteAnswered LayoutStore::write(const std::string& refused, const SettingsWriteRequested& asked) {
    load_prefs();
    const auto refuse = [](std::string why, std::int64_t revision) {
        return SettingsWriteAnswered{false, std::move(why), revision, {}, {}};
    };
    // JUDGED WHOLE, NOTHING WRITTEN ON A REFUSAL: the asker, the revision, the bounds, then every
    // write's form, target, layout and value, and every baseline's.
    if (!refused.empty()) {
        return refuse("settings writes withheld: " + refused, 0);
    }
    const std::int64_t current = layout_settings_revision();
    if (asked.revision != current) {
        return refuse("stale revision " + std::to_string(asked.revision) +
                          " -- the layouts stand at " + std::to_string(current) +
                          "; read them again",
                      current);
    }
    if (asked.writes.size() > kMaxLayouts * kMaxPaneSettingsPerDesk ||
        asked.baselines.size() > kMaxLayouts) {
        return refuse("a batch holds at most " +
                          std::to_string(kMaxLayouts * kMaxPaneSettingsPerDesk) + " writes and " +
                          std::to_string(kMaxLayouts) + " baselines",
                      current);
    }
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
            wrong = gone(w.layout);
        } else if (wrong.empty() && !w.clear) {
            if (const v2::PaneSettingRow* row = declared_row(*ref, w.key)) {
                wrong = v2::pane_setting_refused_by(*row, candidate);
            }
        }
        if (!wrong.empty()) {
            return refuse(at + wrong, current);
        }
    }
    for (std::size_t i = 0; i < asked.baselines.size(); ++i) {
        const LayoutThemeWrite& b = asked.baselines[i];
        const std::string at = "baseline " + std::to_string(i) + ": ";
        std::string wrong;
        if (!place_of(b.layout)) {
            wrong = gone(b.layout);
        } else if (b.theme.name.empty() && b.theme.fingerprint != 0) {
            wrong = "a baseline of no theme has no fingerprint";
        } else if (!b.theme.name.empty()) {
            wrong = layout_theme_name_problem(b.theme.name);
        }
        if (!wrong.empty()) {
            return refuse(at + wrong, current);
        }
    }
    // WRITTEN TOGETHER, ON COPIES: a write a lock holds, a key nothing takes, or a pane its layout
    // does not seat is passed over and said; a bound the batch passes refuses it whole.
    SetupState next = run_;
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
            const bool office_moved_now = prefs_persist::put_office_value(kept, w.target, w.key, value);
            results.push_back({index, office_moved_now ? kWriteWritten : kWriteUnchanged,
                               std::string()});
            if (office_moved_now) {
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
            std::find(desk.locks.begin(), desk.locks.end(), SetupLock{ref, w.key}) !=
                desk.locks.end()) {
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
            return refuse("write " + std::to_string(i) + ": " + wrote.refusal, current);
        }
        const bool row_moved = pane_of(desk, ref)->settings != before;
        results.push_back({index, row_moved ? kWriteWritten : kWriteUnchanged, std::string()});
        if (row_moved && place == live_at(run_)) {
            note(live_moved, w.target);
        }
    }
    // A LAYOUT'S BASELINE IS ITS DESK'S.
    bool baselines_moved = false;
    for (const LayoutThemeWrite& b : asked.baselines) {
        Setup& desk = desk_at(next, *place_of(b.layout));
        baselines_moved = baselines_moved || !(desk.theme == b.theme);
        desk.theme = b.theme;
    }
    if (const std::string wrong = prefs_persist::kept_problem(kept);
        !(kept == prefs_) && !wrong.empty()) {
        return refuse("the settings file would not keep what the batch writes: " + wrong, current);
    }
    // WRITTEN TO THE SETTINGS FILE FIRST: a batch the file does not hold is no batch.
    if (!(kept == prefs_)) {
        const prefs_persist::Prefs before = prefs_;
        prefs_ = std::move(kept);
        if (const std::string not_written = write_prefs();
            !not_written.empty() && !host_->prefs_path.empty()) {
            prefs_ = before;
            return refuse("nothing written: Workshop's settings file was not written -- " +
                              not_written,
                          current);
        }
    }
    const bool any_desk = !same_run(next, run_);
    run_ = std::move(next);
    // HANDED AND APPLIED, and the asker told what was handed.
    std::vector<std::string> handed = live_moved;
    for (const std::string& office : offices_moved) {
        office_moved(office);
        handed.push_back(office);
    }
    if (any_desk || baselines_moved) {
        moved();
    }
    std::size_t written = 0;
    for (const SettingWriteResult& r : results) {
        written += r.outcome == kWriteWritten ? 1u : 0u;
    }
    say((asked.cause.empty() ? std::string("settings") : asked.cause) + ": " +
            std::to_string(written) + " of " + std::to_string(results.size()) + " written" +
            (baselines_moved ? std::string(", baselines set") : std::string()),
        false);
    return SettingsWriteAnswered{true, std::string(), layout_settings_revision(), std::move(results),
                           std::move(handed)};
}

// ---- A desk put live from a caller's text -----------------------------------------------------

std::string LayoutStore::apply_setup(const std::string& text) {
    const setup_persist::LoadedSetup loaded = setup_persist::from_text(text);
    if (!loaded.outcome.accepted) {
        return loaded.outcome.refusal;
    }
    // The first pane the presenter cannot present, by name: a caller preparing its providers
    // learns which one is missing from the owner's own words.
    for (const SetupPane& row : loaded.setup.panes) {
        if (!presentable(row.ref)) {
            return "setup names a pane this Workshop cannot present: " + row.ref.provider + " " +
                   row.ref.pane;
        }
    }
    if (const std::string locked = replacement_refused(loaded.setup); !locked.empty()) {
        return locked;
    }
    Setup desk = kept_locks(loaded.setup);
    if (const Written whole = check_setup(desk); !whole.accepted) {
        return whole.refusal;
    }
    run_.active = std::move(desk);
    run_.active_link = SetupLink{};
    ++run_.put_live;
    settle_live();
    moved();
    say("applied setup " + quoted_setup_name(loaded.setup.name), false);
    return std::string();
}

// WL-LOCK-05 -- agents/workshop/setting-locks.md
std::string LayoutStore::replacement_refused(const Setup& incoming) const {
    // EACH LOCKED VALUE THE INCOMING DESK WOULD CHANGE, named: a key the live desk's row holds
    // locked, here or everywhere, that the incoming row holds otherwise, or a row it would drop.
    const Setup& live = live_desk(run_);
    std::vector<std::string> changed;
    for (const SetupPane& row : live.panes) {
        const SetupPane* next = pane_of(incoming, row.ref);
        for (const SettingLockShown& l : locks_on(row.ref, live_at(run_))) {
            const PaneSetting* now = find_pane_setting(row.settings, l.key);
            const PaneSetting* then = next != nullptr ? find_pane_setting(next->settings, l.key)
                                                      : nullptr;
            const bool same = next != nullptr && ((now == nullptr && then == nullptr) ||
                                                  (now != nullptr && then != nullptr && *now == *then));
            const bool dropped = next == nullptr && l.scope == kLockScopeLayout;
            if ((next != nullptr && !same) || dropped) {
                changed.push_back(l.key + " of " + ref_text(row.ref));
            }
        }
    }
    if (changed.empty()) {
        return std::string();
    }
    std::string named;
    for (const std::string& c : changed) {
        named += (named.empty() ? "" : ", ") + c;
    }
    return "the desk would change what a lock holds -- " + named + "; nothing was replaced";
}

Setup LayoutStore::kept_locks(Setup incoming) const {
    // EVERY LOCK THE LIVE DESK HOLDS STAYS, and a lock the incoming desk brings joins it.
    for (const SetupLock& l : live_desk(run_).locks) {
        if (std::find(incoming.locks.begin(), incoming.locks.end(), l) == incoming.locks.end()) {
            incoming.locks.push_back(l);
        }
    }
    return incoming;
}

// ---- The presenter's edits --------------------------------------------------------------------

// WL-LAYOUT-02 -- agents/workshop/layouts.md
std::string LayoutStore::link_note(const SetupState& s, std::size_t at) const {
    const SetupLink& link = link_at(s, at);
    const std::int64_t status = link_status(layout_at(s, at), link);
    if (status == setup_link::kNone) {
        return {};
    }
    return std::string(" -- ") + link.path + " is " +
           (status == setup_link::kCurrent ? "current" : "modified");
}

std::string LayoutStore::layout_note(const SetupState& s) const {
    return "layout " + quoted_setup_name(live_desk(s).name) + " -- " +
           std::to_string(live_at(s) + 1) + " of " + std::to_string(layout_count(s));
}

namespace {

/// The panes of `desk` the presenter cannot present now, as a clause: empty for none.
// WL-PANE-10 -- agents/workshop/panes-and-windows.md
template <class Presentable>
std::string unresolved_clause(const Setup& desk, const Presentable& presentable) {
    std::vector<PaneRef> waiting;
    for (const SetupPane& row : desk.panes) {
        if (!presentable(row.ref)) {
            waiting.push_back(row.ref);
        }
    }
    if (waiting.empty()) {
        return {};
    }
    std::string note = " -- " + std::to_string(waiting.size()) +
                       (waiting.size() == 1 ? " pane" : " panes") + " unresolved: " +
                       ref_text(waiting.front());
    if (waiting.size() > 1) {
        note += ", ...";
    }
    return note;
}

} // namespace

std::string LayoutStore::edit_one(SetupState& next, const LayoutEdit& e, LayoutEditAnswered& out,
                                  std::vector<std::string>& said, bool& run_moved,
                                  std::vector<std::string>& renamed_from,
                                  std::vector<std::string>& renamed_to,
                                  std::vector<std::string>& removed,
                                  std::optional<SetupWrite>& saving) {
    using namespace layout_op;
    Setup& live = live_desk(next);
    const PaneRef ref{e.provider, e.pane};
    const auto presentable_ref = [this](const PaneRef& r) { return presentable(r); };
    const auto row_op = [&]() -> std::string {
        if (const Written legal = check_pane_ref(ref); !legal.accepted) {
            return legal.refusal;
        }
        return std::string();
    };
    if (e.op == kSeat) {
        if (std::string wrong = row_op(); !wrong.empty()) {
            return wrong;
        }
        if (has_pane(live, ref)) {
            return std::string(); // seated already: the desk is as asked
        }
        if (live.panes.size() >= kMaxSetupPanes) {
            return "a setup holds at most " + std::to_string(kMaxSetupPanes) + " panes";
        }
        (void)add_pane(live, ref);
        return std::string();
    }
    if (e.op == kUnseat) {
        if (std::string wrong = row_op(); !wrong.empty()) {
            return wrong;
        }
        const SetupPane* row = pane_of(live, ref);
        if (row == nullptr) {
            return ref_text(ref) + " is not on this desk";
        }
        out.discarded = row->settings;
        for (const SetupLock& lock : live.locks) {
            if (lock.ref == ref) {
                out.unlocked.push_back(lock.key);
            }
        }
        (void)remove_pane(live, ref);
        return std::string();
    }
    if (e.op == kWindow) {
        if (std::string wrong = row_op(); !wrong.empty()) {
            return wrong;
        }
        SetupPane* row = pane_of(live, ref);
        if (row == nullptr) {
            return "`" + ref_text(ref) + "` is not in this setup";
        }
        PanePlace place;
        PaneSize width;
        PaneSize height;
        if (const Written p = setup_persist::place_in(e.place, place); !p.accepted) {
            return p.refusal;
        }
        if (const Written w = setup_persist::size_in(e.width, width, "width"); !w.accepted) {
            return w.refusal;
        }
        if (const Written h = setup_persist::size_in(e.height, height, "height"); !h.accepted) {
            return h.refusal;
        }
        if (const Written p = check_pane_place(place); !p.accepted) {
            return p.refusal;
        }
        if (const Written w = check_pane_size(width, "width"); !w.accepted) {
            return w.refusal;
        }
        if (const Written h = check_pane_size(height, "height"); !h.accepted) {
            return h.refusal;
        }
        row->place = place;
        row->width = width;
        row->height = height;
        return std::string();
    }
    if (e.op == kFront || e.op == kBack || e.op == kRaise || e.op == kLower) {
        if (std::string wrong = row_op(); !wrong.empty()) {
            return wrong;
        }
        if (!has_pane(live, ref)) {
            return ref_text(ref) + " is no longer in this setup -- the Pane Manager can bring it "
                                   "back";
        }
        const bool did = e.op == kFront  ? send_to_front(live, ref)
                         : e.op == kBack ? send_to_back(live, ref)
                         : e.op == kRaise ? raise_one(live, ref)
                                          : lower_one(live, ref);
        if (!did) {
            return ref_text(ref) + " is already where that would put it";
        }
        return std::string();
    }
    if (e.op == kResetFront) {
        reset_front(live);
        return std::string();
    }
    // ---- The run's ops: a layout named by its id, 0 the live one ----
    const auto at_of = [&next](std::int64_t id) { return position_of(next, id); };
    if (e.op == kSwitch) {
        const std::optional<std::size_t> to = at_of(e.layout);
        if (!to) {
            return "that layout is gone -- nothing was switched";
        }
        const bool switched = activate_layout(next, *to);
        run_moved = run_moved || switched;
        said.push_back(layout_note(next) + link_note(next, live_at(next)) +
                       (switched ? unresolved_clause(live_desk(next), presentable_ref)
                                 : std::string()));
        return std::string();
    }
    if (e.op == kStep) {
        if (layout_count(next) <= 1) {
            said.push_back("this is the only layout");
            return std::string();
        }
        (void)activate_layout(next, layout_step(next, e.by));
        run_moved = true;
        said.push_back(layout_note(next) + link_note(next, live_at(next)) +
                       unresolved_clause(live_desk(next), presentable_ref));
        return std::string();
    }
    if (e.op == kNew) {
        if (!add_layout(next)) {
            return "that is the most layouts one Workshop keeps (" + std::to_string(kMaxLayouts) +
                   ")";
        }
        run_moved = true;
        said.push_back("new " + layout_note(next) + " -- the desk a fresh Workshop opens on");
        return std::string();
    }
    if (e.op == kDuplicate) {
        const std::optional<std::size_t> at = at_of(e.layout);
        if (!at) {
            return "that layout is gone -- nothing was copied";
        }
        if (!duplicate_layout(next, *at)) {
            return "that is the most layouts one Workshop keeps (" + std::to_string(kMaxLayouts) +
                   ")";
        }
        run_moved = true;
        said.push_back("duplicated " + layout_note(next) +
                       " -- the desk was copied, its setup file was not");
        return std::string();
    }
    if (e.op == kRemove) {
        const std::optional<std::size_t> at = at_of(e.layout);
        if (!at) {
            return "that layout is gone -- nothing was removed";
        }
        const std::string name = layout_at(next, *at).name;
        if (!remove_layout(next, *at)) {
            return "this is the only layout -- Workshop always has one desk";
        }
        run_moved = true;
        removed.push_back(name);
        said.push_back("removed layout " + quoted_setup_name(name) + " -- now on " +
                       layout_note(next));
        return std::string();
    }
    if (e.op == kMove) {
        const std::optional<std::size_t> from = at_of(e.layout);
        if (!from) {
            return "that layout is gone -- nothing was moved";
        }
        const std::size_t n = layout_count(next);
        if (e.by < 0 || static_cast<std::size_t>(e.by) >= n) {
            return "no place " + std::to_string(e.by) + " in a run of " + std::to_string(n);
        }
        const std::size_t to = static_cast<std::size_t>(e.by);
        const std::string moved_name = quoted_setup_name(layout_at(next, *from).name);
        if (*from == to) {
            said.push_back("layout " + moved_name + " is already at " + std::to_string(to + 1) +
                           " of " + std::to_string(n));
            return std::string();
        }
        (void)move_layout(next, *from, to);
        run_moved = true;
        said.push_back("moved layout " + moved_name + " to " + std::to_string(to + 1) + " of " +
                       std::to_string(n));
        return std::string();
    }
    if (e.op == kRename) {
        const std::optional<std::size_t> at = at_of(e.layout);
        if (!at) {
            return "that layout is no longer here -- nothing was renamed";
        }
        if (const Written legal = check_setup_name(e.name); !legal.accepted) {
            return legal.refusal;
        }
        Setup candidate = layout_at(next, *at);
        const std::string was = candidate.name;
        candidate.name = e.name;
        if (const Written whole = check_setup(candidate); !whole.accepted) {
            return whole.refusal;
        }
        (void)rename_layout(next, *at, e.name);
        run_moved = true;
        renamed_from.push_back(was);
        renamed_to.push_back(e.name);
        said.push_back("renamed layout " + quoted_setup_name(e.name) + link_note(next, *at));
        return std::string();
    }
    if (e.op == kSave) {
        // ONE FILE A BATCH, written by `edit` once every edit is judged.
        if (saving) {
            return "one edit saves one setup file";
        }
        const std::string path =
            next.active_link.path.empty() ? host_->setup_path : next.active_link.path;
        if (path.empty()) {
            return kNoSetupFile;
        }
        if (const Written whole = check_setup(next.active); !whole.accepted) {
            return whole.refusal;
        }
        saving = SetupWrite{path, next.active};
        next.active_link.path = path;
        const Setup desk = next.active;
        adopt_known_setup(next, path, desk);
        run_moved = true;
        said.push_back("saved setup " + quoted_setup_name(desk.name) + " to " + path +
                       unresolved_clause(desk, presentable_ref));
        return std::string();
    }
    if (e.op == kRestore) {
        const std::string path =
            next.active_link.path.empty() ? host_->setup_path : next.active_link.path;
        if (path.empty()) {
            return kNoSetupFile;
        }
        const setup_persist::LoadedSetup loaded = setup_persist::load_file(path);
        if (!loaded.outcome.accepted) {
            return loaded.outcome.refusal;
        }
        if (const std::string locked = replacement_refused(loaded.setup); !locked.empty()) {
            return locked;
        }
        Setup desk = kept_locks(loaded.setup);
        if (const Written whole = check_setup(desk); !whole.accepted) {
            return whole.refusal;
        }
        next.active = std::move(desk);
        next.active_link.path = path;
        ++next.put_live; // the file's desk, not the one it replaced (WL-INFO-14)
        adopt_known_setup(next, path, loaded.setup);
        run_moved = true;
        said.push_back("restored setup " + quoted_setup_name(loaded.setup.name) + " from " + path +
                       unresolved_clause(loaded.setup, presentable_ref) +
                       (loaded.converted.total() > 0
                            ? "; " + pane_migration::converted_note(loaded.converted)
                            : std::string()));
        return std::string();
    }
    return "`" + e.op + "` is not a layout edit";
}

LayoutEditAnswered LayoutStore::edit(const LayoutEditRequested& asked) {
    LayoutEditAnswered out;
    if (asked.edits.size() > kMaxLayoutEdits) {
        out.refusal = "an edit holds at most " + std::to_string(kMaxLayoutEdits) + " changes";
        out.revision = revision_;
        return out;
    }
    // WHICH ANSWER: an edit judged from a picture the run has moved past is refused whole, and the
    // next picture is the current one.
    if (asked.revision != 0 && asked.revision != revision_) {
        out.refusal = "the layout changed -- the next picture is the current one";
        out.revision = revision_;
        return out;
    }
    SetupState next = run_;
    std::vector<std::string> said;
    bool run_moved = false;
    std::vector<std::string> renamed_from;
    std::vector<std::string> renamed_to;
    std::vector<std::string> removed;
    std::optional<SetupWrite> saving;
    const auto refuse = [&](const std::string& why) {
        out.accepted = false;
        out.refusal = why;
        out.discarded.clear();
        out.unlocked.clear();
        out.revision = revision_;
        return out;
    };
    for (const LayoutEdit& e : asked.edits) {
        const std::string wrong =
            edit_one(next, e, out, said, run_moved, renamed_from, renamed_to, removed, saving);
        if (!wrong.empty()) {
            return refuse(wrong);
        }
    }
    // THE WHOLE JUDGED, ITS FILE WRITTEN: a write that fails refuses the whole, and the last good
    // setup file, every association and the run stand as they were.
    if (saving) {
        if (const Written written = setup_persist::save_file(saving->path, saving->desk);
            !written.accepted) {
            return refuse(written.refusal);
        }
    }
    const bool desk_moved = !same_run(next, run_);
    run_ = std::move(next);
    if (desk_moved || run_moved) {
        settle_live();
        moved();
    }
    if (!said.empty() && effects_.said.empty()) {
        std::string joined;
        for (const std::string& s : said) {
            joined += (joined.empty() ? "" : "; ") + s;
        }
        say(joined, false);
    }
    out.accepted = true;
    out.revision = revision_;
    return out;
}

// ---- A close ----------------------------------------------------------------------------------

// WL-DESK-12 -- agents/workshop/desktop.md
// WL-SETTING-06 -- agents/workshop/settings.md
PaneCloseAnswered LayoutStore::close(const PaneRef& ref) {
    PaneCloseAnswered out;
    out.office = ref.provider;
    out.pane = ref.pane;
    const std::string name = name_of(ref);
    // THE DESK IS WHAT IS ASKED, NOT THE CATALOG: a row the weaver authored is theirs to take off
    // whether or not anything offers it -- an unavailable pane's row is exactly the intent a
    // weaver closes to stop asking for it.
    if (!has_pane(live_desk(run_), ref)) {
        out.refusal = name + " is not on this desk -- nothing to hide, and a hide shows nothing";
        say(out.refusal, true);
        return out;
    }
    LayoutEdit unseat;
    unseat.op = layout_op::kUnseat;
    unseat.provider = ref.provider;
    unseat.pane = ref.pane;
    LayoutEditRequested asked;
    asked.edits.push_back(std::move(unseat));
    const LayoutEditAnswered taken = edit(asked);
    if (!taken.accepted) {
        out.refusal = taken.refusal;
        say(out.refusal, true);
        return out;
    }
    // THE ROW WENT WHOLE: its place, its size, its settings and this layout's locks and pins on
    // it. Nothing behind it is touched -- a provider's lifetime is realization's, not the desk's.
    out.closed = true;
    out.settings = taken.discarded;
    out.locks = taken.unlocked;
    say("hid " + name + discarded_settings(out.settings) + discarded_locks(out.locks) +
            " -- its provider and what it holds are untouched; showing it brings it back",
        false);
    return out;
}

// ---- The last session -------------------------------------------------------------------------

// WL-SESSION-14, WL-SESSION-16, WL-SESSION-17 -- agents/workshop/session-restore.md
// WL-MIG-10 -- agents/workshop/migration.md
RestoredRun LayoutStore::restore() {
    RestoredRun out;
    if (restored_) {
        return out;
    }
    // ONCE PER PROCESS: a window started again asks again, and a weaver whose afternoon of
    // arranging was thrown back to a file written last night would have met a continuity feature
    // that loses work.
    restored_ = true;
    load_prefs();
    if (host_->session_path.empty()) {
        return out; // no session file was chosen: restore nothing, and say nothing about it
    }
    const session_persist::LoadedSession last =
        session_persist::load_file(host_->session_path, host_->conversions);
    if (last.present && !last.outcome.accepted) {
        // AND THIS RUN WILL NOT WRITE OVER IT: an orderly close would replace bytes this run could
        // not read. The refusal is held all run.
        session_refusal_ = last.outcome.refusal;
        moved();
        return out;
    }
    if (last.present) {
        // The whole run comes back, each layout with the id it was minted, exactly one live.
        (void)install_layout_run(run_, last.layouts, last.active);
        run_.next_id = std::max(run_.next_id, last.next_id);
        out.present = true;
        out.read_version = last.read_version;
        out.head = "reopened your last desk " + quoted_setup_name(live_desk(run_).name);
        if (last.layouts.size() > 1) {
            out.head += " (" + std::to_string(last.active + 1) + " of " +
                        std::to_string(last.layouts.size()) + " layouts)";
        }
        if (last.converted.total() > 0) {
            out.converted = pane_migration::converted_note(last.converted);
        }
    }
    // WHAT THE SETTINGS FILE KEPT FOR LAYOUTS BY NAME: a session of this version holds it already;
    // any other run takes it now, and the file carries it until a session holds it.
    bool folded = false;
    if (last.present && last.read_version == session_persist::kFormatVersion) {
        layout_entries_held_ = true;
    } else if (prefs_persist::has_layout_entries(prefs_)) {
        out.folded = fold_layout_entries();
        folded = true;
    }
    if (last.present || folded) {
        settle_live();
        moved();
    }
    return out;
}

// WL-SETTING-11 -- agents/workshop/settings.md
std::string LayoutStore::fold_layout_entries() {
    // EACH LAYOUT NAMED N TAKES THE BASELINE, LOCKS AND PINS KEPT UNDER N, for rows it seats;
    // layouts sharing a name each take a copy; what no layout takes is named, each entry in full.
    std::vector<std::string> shared;
    std::vector<std::string> dropped;
    const std::vector<Layout> before = layout_run(run_);
    const auto layout_of = [](const std::string& target) {
        const std::size_t at = target.find('@');
        return at == std::string::npos ? std::string() : target.substr(at + 1);
    };
    const auto takers = [&](const std::string& name) {
        std::vector<std::size_t> out;
        for (std::size_t i = 0; i < before.size(); ++i) {
            if (before[i].desk.name == name) {
                out.push_back(i);
            }
        }
        return out;
    };
    const auto fold = [&](const std::string& name, const std::string& entry,
                          const auto& into) {
        const std::vector<std::size_t> at = takers(name);
        bool taken = false;
        for (const std::size_t i : at) {
            Setup candidate = layout_at(run_, i);
            if (into(candidate) && check_setup(candidate).accepted) {
                desk_at(run_, i) = std::move(candidate);
                taken = true;
            }
        }
        if (!taken) {
            dropped.push_back(entry);
        } else if (at.size() > 1 &&
                   std::find(shared.begin(), shared.end(), name) == shared.end()) {
            shared.push_back(name);
        }
    };
    for (const prefs_persist::OfficeSettingsKept& o : prefs_.offices) {
        if (!prefs_persist::is_layout_theme_entry(o.office)) {
            continue;
        }
        const std::string name = o.office.substr(1);
        const LayoutTheme theme = prefs_persist::layout_theme_of(prefs_, name);
        fold(name, "the baseline of layout " + quoted_setup_name(name) + " (" + theme.name + ")",
             [&theme](Setup& desk) {
                 desk.theme = theme;
                 return true;
             });
    }
    for (const prefs_persist::PinList& l : prefs_.pins) {
        const std::string name = layout_of(l.target);
        const std::optional<PaneRef> ref = pane_of_target(l.target.substr(0, l.target.find('@')));
        if (name.empty()) {
            continue;
        }
        fold(name, "the pins of `" + l.target + "`", [&](Setup& desk) {
            if (!ref || pane_of(desk, *ref) == nullptr) {
                return false;
            }
            std::erase_if(desk.pins, [&](const SetupPins& p) { return p.ref == *ref; });
            desk.pins.push_back(SetupPins{*ref, l.keys});
            return true;
        });
    }
    for (const SettingLock& l : prefs_.locks) {
        const std::string name = layout_of(l.target);
        const std::optional<PaneRef> ref = pane_of_target(l.target.substr(0, l.target.find('@')));
        if (name.empty()) {
            continue;
        }
        fold(name, "the lock on " + l.key + " of `" + l.target + "`", [&](Setup& desk) {
            if (!ref || pane_of(desk, *ref) == nullptr) {
                return false;
            }
            const SetupLock lock{*ref, l.key};
            if (std::find(desk.locks.begin(), desk.locks.end(), lock) == desk.locks.end()) {
                desk.locks.push_back(lock);
            }
            return true;
        });
    }
    std::string said;
    for (const std::string& name : shared) {
        said += (said.empty() ? "" : "; ") + std::string("layouts named ") +
                quoted_setup_name(name) + " each keep their own locks, pins and baseline now";
    }
    if (!dropped.empty()) {
        std::string named;
        for (const std::string& d : dropped) {
            named += (named.empty() ? "" : ", ") + d;
        }
        said += (said.empty() ? "" : "; ") +
                std::string("the settings file kept for layouts this session does not seat, and "
                            "they were not carried: ") +
                named;
    }
    return said;
}

// WL-SESSION-13, WL-SESSION-15 -- agents/workshop/session.md
std::string LayoutStore::save_session() {
    if (host_->session_path.empty()) {
        return "isolated run -- no session file";
    }
    if (!session_refusal_.empty()) {
        return host_->session_path + " could not be read and will not be written over";
    }
    const Written written = session_persist::save_file(
        host_->session_path, layout_run(run_), live_at(run_), run_.next_id,
        host_->chosen ? host_->chosen() : std::vector<session_persist::WorkshopChoice>{});
    if (!written.accepted) {
        return written.refusal;
    }
    // THE SESSION HOLDS WHAT THE SETTINGS FILE KEPT FOR LAYOUTS: the file's own copies go now.
    layout_entries_held_ = true;
    if (prefs_persist::has_layout_entries(prefs_) && !prefs_bad_) {
        (void)write_prefs();
    }
    return std::string();
}

} // namespace zengine::workshop
