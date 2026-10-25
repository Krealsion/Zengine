// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LAYOUTS_STORE_HPP
#define ZENGINE_WORKSHOP_LAYOUTS_STORE_HPP

// The layouts' store, as a value: every layout of the run and what each keeps, every office's
// office-wide settings, the locks and the pins; what each pane and office declares it takes; the
// revisions every reading stands at; what each pane was handed of its row and said it cannot use;
// and the files all of it is kept in: their one writer and judge. What an ask said and moved is
// taken after it (`take_effects`), for the holder to say and hand.
// Workshop law: agents/workshop/settings.md (+3 registers; agents/workshop.md routes)

#include "workshop/desktop_seam_vocabulary.hpp" // a close's answer
#include "workshop/layouts/run.hpp"
#include "workshop/layouts/vocabulary.hpp"
#include "workshop/pane_migration.hpp"
#include "workshop/prefs_persist.hpp"
#include "workshop/layouts/session_persist.hpp"
#include "workshop/settings_seam_vocabulary.hpp"
#include "workshop/setup_persist.hpp"

#include "operator/catalog.hpp"

#include <zen/schema.hpp>
#include <zen/switchboard/message.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop {

/// WHAT THE STORE IS HANDED by whoever holds it: the paths of its files, the conversions this run
/// has, and the readings of the bus it spends -- whose office it is now, at which incarnation,
/// and whether its holder takes a shape -- each read at the ask and kept nowhere.
struct LayoutsHost {
    std::string setup_path;
    std::string session_path;
    std::string prefs_path;
    const op::Catalog* conversions = nullptr;
    std::function<loom::WeaveId(std::string_view)> role_holder;
    std::function<std::int64_t(loom::WeaveId)> incarnation_of;
    std::function<bool(std::string_view role, const loom::Schema& shape)> holder_accepts;
    /// Which authored choice holds each switchable office now, written into the session.
    // WL-SWITCH-08 -- agents/workshop/editor-switch.md
    std::function<std::vector<session_persist::WorkshopChoice>()> chosen;
};

/// ONE PANE THE CATALOG LISTS, as the store reads it: its name, and whether the presenter can
/// present it now.
struct CatalogName {
    PaneRef ref;
    std::string name;
    bool presentable = false;
};

/// WHAT ONE ASK OF THE STORE SAID AND MOVED: its sentence for the notice, whether that is a
/// refusal, whether the run or the live layout moved, what a live row's pane cannot use moved,
/// and the offices whose settings moved.
struct StoreEffects {
    std::string said;
    bool bad = false;
    std::string said_provider; ///< the row `said` names, when it is a setting written
    std::string said_pane;
    bool live_moved = false;
    bool unused_moved = false;
    std::vector<std::string> offices;
};

/// ONE HAND-OFF OWED: the row's pane, the weave that holds its office, whether that holder takes
/// v2, and the row's settings without Workshop's own keys at the revision they stand at.
struct PaneHandOff {
    PaneRef ref;
    loom::WeaveId holder{};
    bool revisions = false;
    v2::PaneSettings value;
};

/// WHAT THE LAST SESSION'S RESTORE BROUGHT BACK, for the presenter to say: whether a run came
/// back, its sentence, the panes that changed hands, and what the settings file kept for layouts
/// that was folded into the run or not carried.
struct RestoredRun {
    bool present = false;
    std::string head;      ///< "reopened your last desk ..." -- empty when nothing came back
    std::string converted; ///< a pane that changed hands, said once for the whole run, or empty
    std::string folded;    ///< the settings file's layouts' entries, folded and dropped, or empty
    std::int64_t read_version = 0; ///< the session version read, 0 for none
};

/// The store.
class LayoutStore {
public:
    explicit LayoutStore(const LayoutsHost& host) : host_(&host) {}

    // ---- The run --------------------------------------------------------------------------

    /// Every layout of the run, and which one is live.
    const SetupState& run() const noexcept { return run_; }
    /// The revision the run and the live layout stand at: it moves on any change of either.
    std::int64_t revision() const noexcept { return revision_; }
    /// THE LIVE LAYOUT AS ITS PRESENTER NEEDS IT, each live row's settings revision minted now.
    LiveLayout live_layout();
    /// THE RUN PUT IN PLACE WHOLE, its live desk settled and presented again.
    void put_run(SetupState run);

    // ---- The catalog and what each pane and office takes -----------------------------------

    /// The panes the presenter's catalog lists now: names for sentences, and what it can present.
    void set_catalog(std::vector<CatalogName> names) { catalog_ = std::move(names); }
    /// A pane's name as the catalog says it, else `provider/pane`.
    std::string name_of(const PaneRef& ref) const;
    /// Whether the presenter can present the pane `ref` names.
    bool presentable(const PaneRef& ref) const;

    /// A PANE'S DECLARATION, from `office` by the weave `from`: judged whole, then kept in place of
    /// the last one, counting while `from` holds the office at the incarnation it has now. Empty
    /// when kept, else the refusal.
    std::string declare_pane(std::string_view office, const std::string& pane,
                             std::vector<v2::PaneSettingRow> rows, loom::WeaveId from);
    /// The settings `ref`'s declaration gives while it counts, or nullptr.
    const std::vector<v2::PaneSettingRow>* counted(const PaneRef& ref) const;
    /// ...and the same rows as v1 says them.
    const std::vector<PaneSettingRow>* counted_v1(const PaneRef& ref) const;
    /// AN OFFICE'S OFFICE-WIDE DECLARATION, the same way. Empty when kept, else the refusal.
    std::string declare_office(const std::string& office, const std::vector<v2::PaneSettingRow>& rows,
                               loom::WeaveId from);
    /// What an office's declaration gives: Workshop's own, declared in code, or the counted
    /// declaration of an office that sent one; nullptr when none counts.
    const std::vector<v2::PaneSettingRow>* office_rows(std::string_view office) const;

    // ---- Revisions -------------------------------------------------------------------------

    /// THE REVISION THE LIVE ROW FOR `ref` STANDS AT, minted afresh when what it was minted for
    /// moved -- the desk put live, the row's settings or the counted declaration; 0 when the live
    /// layout has no such row.
    std::int64_t row_revision(const PaneRef& ref);
    /// THE REVISION AN OFFICE'S SETTINGS STAND AT, the same way; 0 when nothing declares.
    std::int64_t office_revision(const std::string& office);
    /// The revision every layout's settings stand at, minted afresh whenever the reading moved.
    std::int64_t layout_settings_revision();

    // ---- The doors -------------------------------------------------------------------------
    // `refused` is the asker's standing, judged by the holder (WL-SETTING-09): empty when this
    // asker is served, else why not.

    PaneSettingsAnswered read_pane(const std::string& refused, const PaneSettingsRequested& asked);
    PaneSettingCommitAnswered commit_pane(const std::string& refused, const PaneSettingCommitRequested& asked);
    OfficeSettingsAnswered read_office(const std::string& refused, const OfficeSettingsRequested& asked);
    OfficeSettingCommitAnswered commit_office(const std::string& refused,
                                              const OfficeSettingCommitRequested& asked);
    PaneSettingPinAnswered pin(const std::string& refused, const PaneSettingPinRequested& asked);
    /// `weavers_hand`: the unlock comes from the weaver's own hand, as the holder judged it.
    PaneSettingLockAnswered lock(const std::string& refused, const PaneSettingLockRequested& asked,
                              bool weavers_hand);
    LayoutSettingsAnswered read_layouts(const std::string& refused);
    SettingsWriteAnswered write(const std::string& refused, const SettingsWriteRequested& asked);
    /// A DESK PUT LIVE IN PLACE OF THE LIVE ONE'S, from the setup text a caller sent: empty when
    /// applied, else the refusal.
    std::string apply_setup(const std::string& text);

    /// THE PRESENTER'S EDITS, written whole or not at all.
    LayoutEditAnswered edit(const LayoutEditRequested& asked);
    /// A CLOSE: the live layout's row for `ref` taken off whole, its settings and this layout's
    /// locks on it named, and said; a row the live layout does not seat refused.
    PaneCloseAnswered close(const PaneRef& ref);

    /// What an office's holder is handed now, for each declaring office whose holder takes
    /// `OfficeSettings` and was not handed these: the office and the value.
    std::vector<std::pair<std::string, OfficeSettings>> office_hand_offs();
    /// ...and that one of them was handed.
    void office_handed(const std::string& office, std::int64_t revision);

    // ---- A row's settings, handed to its pane ----------------------------------------------

    /// WHAT EACH PANE OF THE LIVE LAYOUT IS HANDED NOW: every row whose pane some holder offers
    /// and whose office's holder takes `PaneSettings`, in the newest version it takes, when what
    /// that holder was last handed differs -- another holder, another revision for v2, other
    /// settings; and each pane that asked while the live layout seats it nowhere, its settings at
    /// their defaults, at revision 0.
    std::vector<PaneHandOff> pane_hand_offs();
    /// ...and that one of them was handed: what its holder holds now.
    void pane_handed(const PaneHandOff& handed);
    /// A PANE ASKED FOR ITS ROW: the life that holds its office holds nothing it was handed, and
    /// is handed its row now -- or, while the live layout seats it nowhere, every setting at its
    /// default.
    void pane_asked(const PaneRef& ref);
    /// WHAT A PANE SAYS IT CANNOT USE, from `from` as `office`: kept for the revision its holder
    /// was last handed, from that holder alone, and refused aloud when it cannot have meant it.
    void keep_unused(std::string_view office, loom::WeaveId from, const PaneSettingUnused& unused);
    /// What the pane of `ref` said it cannot use of what its holder was last handed.
    std::vector<RowSettingUnused> unused_of(const PaneRef& ref) const;
    /// HOW FAR THE HAND-OFF OF THE LIVE ROW FOR `ref` GOT, in the band's words: stored and handed,
    /// with what the pane cannot use of it, or why it was not handed.
    std::string handed_fact(const PaneRef& ref);

    // ---- The files -------------------------------------------------------------------------

    /// READ THE SETTINGS FILE, once.
    void load_prefs();
    /// What the settings file keeps, as this run holds it.
    const prefs_persist::Prefs& prefs() const noexcept { return prefs_; }
    /// Whether it could be read; when not, why.
    const std::string& prefs_refusal() const noexcept { return prefs_refusal_; }
    /// RESTORE THE LAST SESSION, once per process: the run, and what the settings file kept for
    /// its layouts folded into it.
    RestoredRun restore();
    /// Whether the last session could not be read, and why; held all run.
    const std::string& session_refusal() const noexcept { return session_refusal_; }
    /// WRITE THE SESSION, at an orderly close: the run and the choices. Empty when written, else
    /// why not. Once a session holds what the settings file kept for layouts, the file drops it.
    std::string save_session();
    /// Whether the settings file still carries layouts' entries no written session holds.
    bool carries_layout_entries() const;

    /// What the last asks said and moved, taken: the holder says, presents and hands it.
    StoreEffects take_effects();
    /// A DECLARATION REFUSED, said as the store's own refusal.
    void refuse_declaration(std::string why) { say(std::move(why), true); }

private:
    struct PaneDeclaration {
        PaneRef ref;
        std::vector<v2::PaneSettingRow> rows;
        std::vector<PaneSettingRow> narrow;
        loom::WeaveId from{};
        std::int64_t incarnation = 0;
    };
    struct OfficeDeclaration {
        std::string office;
        std::vector<v2::PaneSettingRow> rows;
        loom::WeaveId from{};
        std::int64_t incarnation = 0;
        std::int64_t handed = 0;
    };
    struct SettingsRevision {
        PaneRef ref;
        std::int64_t revision = 0;
        std::uint64_t desk = 0;
        std::vector<PaneSetting> settings;
        std::vector<v2::PaneSettingRow> rows;
    };
    struct OfficeRevision {
        std::string office;
        std::int64_t revision = 0;
        std::vector<v2::PaneSettingRow> rows;
        std::vector<PaneSetting> settings;
        loom::WeaveId holder{};
    };
    /// WHAT ONE PANE'S HOLDER WAS LAST HANDED, and what it said it cannot use of that.
    struct RowHanded {
        PaneRef ref;
        loom::WeaveId holder{};
        std::int64_t revision = 0; ///< 0 for a holder that takes v1 alone
        std::vector<PaneSetting> settings;
        std::vector<RowSettingUnused> unused;
    };
    const RowHanded* handed_of(const PaneRef& ref) const;
    /// The holder that takes the hand-off of `ref` now and whether in v2, or none.
    std::optional<std::pair<loom::WeaveId, bool>> taker_of(const PaneRef& ref) const;

    void say(std::string said, bool bad);
    void moved();
    void office_moved(const std::string& office);
    std::string write_prefs();
    bool counts(loom::WeaveId from, std::int64_t incarnation, std::string_view office) const;
    const v2::PaneSettingRow* declared_row(const PaneRef& ref, const std::string& key) const;

    // The live desk settled after any change of its membership: every everywhere lock pinned on
    // the rows it seats.
    void settle_live();
    bool pin_everywhere_locks(Setup& desk) const;
    std::vector<std::string> effective_pins(const PaneRef& ref, std::size_t at) const;
    std::vector<SettingLockShown> locks_on(const PaneRef& ref, std::size_t at) const;
    std::vector<std::string> locked_keys(const PaneRef& ref) const;
    LayoutSettingsAnswered layout_settings_reading() const;
    std::string replacement_refused(const Setup& incoming) const;
    Setup kept_locks(Setup incoming) const;
    std::string fold_layout_entries();
    std::optional<std::size_t> place_of(std::int64_t layout) const;
    std::string gone(std::int64_t layout) const;

    // The edit's own ops, each on the candidate run; a save names the file and desk it writes once
    // the whole is judged.
    struct SetupWrite {
        std::string path;
        Setup desk;
    };
    std::string edit_one(SetupState& next, const LayoutEdit& e, LayoutEditAnswered& out,
                         std::vector<std::string>& said, bool& run_moved,
                         std::vector<std::string>& renamed_from,
                         std::vector<std::string>& renamed_to, std::vector<std::string>& removed,
                         std::optional<SetupWrite>& saving);
    std::string link_note(const SetupState& s, std::size_t at) const;
    std::string layout_note(const SetupState& s) const;

    const LayoutsHost* host_;
    SetupState run_;
    std::int64_t revision_ = 1;
    std::vector<CatalogName> catalog_;
    std::vector<PaneDeclaration> pane_declarations_;
    std::vector<OfficeDeclaration> office_declarations_;
    std::vector<SettingsRevision> settings_revisions_;
    std::vector<OfficeRevision> office_revisions_;
    std::vector<RowHanded> handed_;
    std::vector<PaneRef> asked_unseated_;
    std::int64_t settings_revision_ = 0;
    LayoutSettingsAnswered layout_settings_minted_;
    std::int64_t layout_settings_revision_ = 0;

    bool prefs_loaded_ = false;
    bool prefs_bad_ = false;
    std::string prefs_refusal_;
    prefs_persist::Prefs prefs_;
    std::string prefs_bytes_;
    bool prefs_file_present_ = false;

    bool restored_ = false;
    std::string session_refusal_;
    /// A session holding the run's layouts' entries was read or written: the settings file's own
    /// copies of them are dropped at its next write.
    bool layout_entries_held_ = false;

    StoreEffects effects_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_LAYOUTS_STORE_HPP
