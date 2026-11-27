// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LAYOUTS_OWNER_HPP
#define ZENGINE_WORKSHOP_LAYOUTS_OWNER_HPP

// The layouts' owner, the role `zengine.layouts`: the store as a weave of its own, mounted by the
// host in its own process and never replaced. It sends its presenter each picture of the live
// layout, answers its edits, writes the session at its orderly close, takes a pane's row off the
// live layout when a role closes it, hands each seated pane its row's settings and hears what
// the pane cannot use, and is every settings door: one writer and one judge of every layout,
// role-wide setting, lock and pin.
// Workshop law: agents/workshop/layouts.md (+2 registers; agents/workshop.md routes)

#include "workshop/actor_scope.hpp" // a guest's row, which says whether it may touch settings
#include "workshop/desktop_seam_vocabulary.hpp" // the presenter's inventory: names, and what it presents
#include "workshop/layouts/store.hpp"
#include "workshop/layouts/vocabulary.hpp"
#include "workshop/settings_seam_vocabulary.hpp"
#include "workshop/setup_control.hpp"

#include "activation/activation.hpp"
#include "manual/vocabulary.hpp"
#include "manuals/zengine.layouts.hpp" // the role's manual, compiled in from its page

#include <zen/weave.hpp>
#include <zen/weave/ask_ended.hpp>

#include <cstdint>
#include <deque>
#include <optional>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {
using pane::PaneSettingUnused;
using pane::PaneSettings;
using pane::PaneSettingsAsked;
using pane::PaneSettingsDeclared;

/// THE RUN AS THE SESSION FILE KEEPS IT: every layout with the id the owner minted it, its desk
/// and its link to a setup file; which one is live; and the id the next one takes.
struct LayoutRun {
    std::vector<session_persist::WorkshopLayout> layouts;
    std::int64_t active = 0;
    std::int64_t next_id = 0;
    ZEN_SHAPE(LayoutRun, 1, ZEN_FIELD(layouts), ZEN_FIELD(active), ZEN_FIELD(next_id));
};

/// WHAT THE OWNER KEEPS THAT MUST OUTLIVE A RELOAD, and what it has done. The run and the
/// settings are its facts, each in its file's own shapes: a reload carries them, and the session
/// and settings files are written from them. Each row's and each role's settings are held back
/// from a poke by their shapes' own tags (`ZEN_HIDE`), and told by the settings doors. The
/// revisions move into the state, so a reader that named a desk or a revision never meets another
/// under that number after a reload. The counters are writable one by one, and nothing else is,
/// so a `zen.PokeResetState`, which needs every field writable, is refused rather than putting
/// the default desk live over every layout the weaver authored.
struct LayoutsOwnerState {
    std::int64_t edits = 0;    ///< edits the presenter asked, an empty one included
    std::int64_t refused = 0;  ///< asks refused before they were judged: the wrong asker
    std::int64_t pictures = 0; ///< pictures of the live layout sent
    std::int64_t saves = 0;    ///< sessions written
    std::int64_t withheld = 0; ///< settings asks from an asker the doors do not serve
    std::int64_t questions = 0; ///< weaver's-act questions asked of the presenter
    std::int64_t revision = 0; ///< moves on any change of the run or of the live layout
    std::int64_t put_live = 0; ///< moves each time another desk is put live
    LayoutRun run;
    prefs_persist::WorkshopPrefs settings; ///< the settings file: every role's, the pins, the locks
    ZEN_SHAPE(LayoutsOwnerState, 2, ZEN_EXPOSE(edits), ZEN_EXPOSE(refused), ZEN_EXPOSE(pictures),
              ZEN_EXPOSE(saves), ZEN_EXPOSE(withheld), ZEN_EXPOSE(questions), ZEN_FIELD(revision),
              ZEN_FIELD(put_live), ZEN_FIELD(run), ZEN_FIELD(settings));
};

/// WHAT THE HOST HANDS THE OWNER AT ITS MOUNT, read at each ask: the files' paths and the
/// conversions. Who is on the bus is Loom's participants claim, which the owner reads in each
/// delivery; a guest's row is the guest door's answer, asked at the act; which weave is the
/// Terminal the host says once by message (`TerminalNamed`).
// WL-SETTING-09 -- agents/workshop/settings.md
struct LayoutsOwnerHost {
    std::function<LayoutsHost()> store;
};

/// THE OWNER'S HOST READING OVER `files`, whatever holds them -- the host's own locals, a suite's
/// rig: its `setup_path`, `session_path`, `prefs_path` and `conversions`, each read at the ask, so
/// `files` outlives the owner.
// WL-SETTING-09 -- agents/workshop/settings.md
template <class Files>
LayoutsOwnerHost layouts_owner_host(const Files& files) {
    LayoutsOwnerHost out;
    out.store = [&files] {
        LayoutsHost readings;
        readings.setup_path = files.setup_path;
        readings.session_path = files.session_path;
        readings.prefs_path = files.prefs_path;
        readings.conversions = files.conversions;
        return readings;
    };
    return out;
}

/// How many settings asks the owner holds at once while the guest door says each asker's row.
inline constexpr std::size_t kMaxHeldSettingsAsks = 16;

/// The owner.
class LayoutsOwner
    : public zengine::With<manual::ManualFacet<manual::pages::zengine_layouts::kManual>>::Weave<
          LayoutsOwner, LayoutsOwnerState,
          loom::Accept<LayoutEditRequested, SessionSaveRequested, SetupApplyRequested,
                       PaneSettingsDeclared, pane::v2::PaneSettingsDeclared, RoleSettingsDeclared,
                       PaneSettingsAsked, PaneSettingUnused, PaneSettingsRequested,
                       PaneSettingCommitRequested, RoleSettingsRequested,
                       RoleSettingCommitRequested, PaneSettingPinRequested,
                       PaneSettingLockRequested, LayoutSettingsRequested, SettingsWriteRequested,
                       PaneInventory, WeaversActAnswered, loom::AskEnded, PaneCloseRequested,
                       // ...and as a pane built on an earlier vocabulary says them
                       v1::PaneCloseRequested, OfficeSettingsDeclared, OfficeSettingsRequested,
                       OfficeSettingCommitRequested,
                       // ...which row admitted a settings asker, and which weave is the Terminal
                       GuestRowOf, TerminalNamed>,
          loom::Emit<LiveLayout, LayoutEditAnswered, SessionSaveAnswered, loom::Ack, loom::Refused,
                     PaneSettings, pane::v2::PaneSettings, PaneSettingsAnswered,
                     PaneSettingCommitAnswered, RoleSettings, RoleSettingsAnswered,
                     RoleSettingCommitAnswered, PaneSettingPinAnswered, PaneSettingLockAnswered,
                     LayoutSettingsAnswered, SettingsWriteAnswered, WeaversActRequested,
                     PaneInventoryRequested, PaneCloseAnswered, v2::PaneCloseAnswered,
                     OfficeSettingsAnswered, OfficeSettingCommitAnswered,
                     GuestRowOfRequested>> {
public:
    explicit LayoutsOwner(LayoutsOwnerHost host) : host_(std::move(host)), store_(readings_) {}

    // ---- The presenter's link ---------------------------------------------------------------
    void on(const LayoutEditRequested& asked, loom::Mail& mail);
    void on(const SessionSaveRequested& asked, loom::Mail& mail);
    void on(const PaneInventory& said, loom::Mail& mail);
    void on(const WeaversActAnswered& answered, loom::Mail& mail);
    /// LOOM'S WORD THAT AN ASK OF THIS OWNER'S ENDED WITH NO ANSWER: an unlock's question to the
    /// presenter is answered refused, so its asker waits on nothing; the inventory's is not waited on.
    void on(const loom::AskEnded& ended, loom::Mail& mail);

    // ---- Who asks ------------------------------------------------------------------------------
    /// THE ROW THAT ADMITTED A HELD ASK'S ASKER, from the guest door: that ask judged and answered.
    void on(const GuestRowOf& row, loom::Mail& mail);
    /// THE HOST SAYS WHICH WEAVE IS THE TERMINAL IT MOUNTED; from a weave it is nothing.
    void on(const TerminalNamed& named, loom::Mail& mail);

    // ---- A desk put live from a caller's text ----------------------------------------------
    void on(const SetupApplyRequested& asked, loom::Mail& mail);

    // ---- A pane's row taken off the live layout ---------------------------------------------
    void on(const PaneCloseRequested& asked, loom::Mail& mail);
    void on(const v1::PaneCloseRequested& asked, loom::Mail& mail);
    PaneCloseAnswered close_asked(const PaneCloseRequested& asked, loom::Mail& mail);

    // ---- What each pane and role takes ----------------------------------------------------
    void on(const PaneSettingsDeclared& declared, loom::Mail& mail);
    void on(const pane::v2::PaneSettingsDeclared& declared, loom::Mail& mail);
    void on(const RoleSettingsDeclared& declared, loom::Mail& mail);
    void on(const OfficeSettingsDeclared& declared, loom::Mail& mail) { on(current(declared), mail); }

    // ---- A row's settings, handed to its pane -----------------------------------------------
    void on(const PaneSettingsAsked& asked, loom::Mail& mail);
    void on(const PaneSettingUnused& unused, loom::Mail& mail);

    // ---- The settings doors -----------------------------------------------------------------
    void on(const PaneSettingsRequested& asked, loom::Mail& mail);
    void on(const PaneSettingCommitRequested& asked, loom::Mail& mail);
    void on(const RoleSettingsRequested& asked, loom::Mail& mail);
    void on(const RoleSettingCommitRequested& asked, loom::Mail& mail);
    // An earlier vocabulary's asks, read as the current ones and answered in their own names.
    void on(const OfficeSettingsRequested& asked, loom::Mail& mail);
    void on(const OfficeSettingCommitRequested& asked, loom::Mail& mail);
    RoleSettingsAnswered read_asked(const RoleSettingsRequested& asked, loom::Mail& mail);
    RoleSettingCommitAnswered commit_asked(const RoleSettingCommitRequested& asked,
                                           loom::Mail& mail);
    void on(const PaneSettingPinRequested& asked, loom::Mail& mail);
    void on(const PaneSettingLockRequested& asked, loom::Mail& mail);
    void on(const LayoutSettingsRequested& asked, loom::Mail& mail);
    void on(const SettingsWriteRequested& asked, loom::Mail& mail);

    /// THE STORE, for a host's rig: what it holds, read; a run put in place past the doors.
    const LayoutStore& layouts() const noexcept { return store_; }
    LayoutStore& store() noexcept { return store_; }
    /// WHAT THE STORE MOVED SINCE THE LAST MESSAGE, sent now: each pane's row, then the picture,
    /// then each role's settings.
    void settle(loom::Mail& mail, bool picture = false);
    /// THE HOST'S READINGS, taken now, and who is on the bus as `now` says: for a rig that reads the
    /// store between messages, where no delivery reads Loom's claim.
    void read_host_now(std::optional<loom::host::Participants> now = std::nullopt) {
        read_host();
        readings_.now = std::move(now);
    }

    /// THE STATE MADE TO SAY WHAT THE STORE HOLDS, at the end of every delivery: the run, the
    /// settings and the revisions.
    void after_delivery(loom::Mail& mail);
    /// A RELOAD'S STATE PUT BACK INTO THE STORE: the run, the settings and the revisions, and
    /// nothing an asker held, whose asks end with `zen.AskEnded`.
    void revive(const loom::Value& v) override;

private:
    struct PendingUnlock {
        std::uint64_t question = 0;
        loom::DeferredAnswer answer;
        PaneSettingLockRequested asked;
        std::string refused;
    };

    /// A SETTINGS ASK HELD WHILE THE GUEST DOOR SAYS WHICH ROW ADMITTED ITS ASKER: run again as it
    /// was asked once the row comes, and answered through the answer right it holds.
    struct HeldAsk {
        loom::WeaveId asker;
        std::uint64_t question = 0;
        loom::DeferredAnswer answer;
        std::function<void(loom::Mail&)> rerun;
    };
    /// Hold `asked` when its asker is no role and not the Terminal: true when it is held, or
    /// refused for want of room, and the handler goes no further.
    template <class Asked>
    bool held_for_row(const Asked& asked, loom::Mail& mail);
    /// Ask the guest door for the first held ask's row; with no door to ask, run it as no guest's.
    void ask_row(loom::Mail& mail);
    /// Run the first held ask with `row`, its asker's row as the door said it.
    void rerun_first(const scope::GuestRowFacts& row, loom::Mail& mail);
    /// Answer the ask being handled: through its held answer right when it is a held ask run again.
    template <class Answer>
    void reply(loom::Mail& mail, Answer&& answer);

    std::string asker_refused(const loom::Mail& mail) const;
    bool from_terminal(const loom::Mail& mail) const;
    bool from_presenter(const loom::Mail& mail) const;
    void send_live(loom::Mail& mail, const StoreEffects& fx);
    void hand_rows(loom::Mail& mail);

    /// THE HOST'S READINGS, read afresh before each ask is judged.
    void read_host();
    void read_host(loom::Mail& mail);

    LayoutsOwnerHost host_;
    LayoutsHost readings_;
    LayoutStore store_;
    std::vector<PendingUnlock> unlocks_;
    /// The asks waiting on the guest door, in the order they came: a person's asks keep theirs.
    std::deque<HeldAsk> held_;
    /// The held ask being run again, and its asker's row as the door said it; none otherwise.
    const HeldAsk* rerunning_ = nullptr;
    scope::GuestRowFacts row_now_;
    /// The Terminal the host said it mounted.
    loom::WeaveId terminal_{};
    std::uint64_t questions_ = 0;
    RestoredRun restored_;
    bool started_ = false;
    bool restore_said_ = false;
};

/// What the owner may say, and to whom: each answer to its asker, a close's among them, the
/// picture and its question to the presenter, each pane's row to its role's holder, and each
/// role's settings to that role.
loom::Grant layouts_grant();

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_LAYOUTS_OWNER_HPP
