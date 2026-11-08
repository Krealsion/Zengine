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

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// WHAT THE OWNER HAS DONE: counters only.
struct LayoutsOwnerState {
    std::int64_t edits = 0;    ///< edits the presenter asked, an empty one included
    std::int64_t refused = 0;  ///< asks refused before they were judged: the wrong asker
    std::int64_t pictures = 0; ///< pictures of the live layout sent
    std::int64_t saves = 0;    ///< sessions written
    std::int64_t withheld = 0; ///< settings asks from an asker the doors do not serve
    std::int64_t questions = 0; ///< weaver's-act questions asked of the presenter
    ZEN_EXPOSE();
    ZEN_SHAPE(LayoutsOwnerState, 1, ZEN_FIELD(edits), ZEN_FIELD(refused), ZEN_FIELD(pictures),
              ZEN_FIELD(saves), ZEN_FIELD(withheld), ZEN_FIELD(questions));
};

/// WHAT THE HOST HANDS THE OWNER AT ITS MOUNT, each read at the ask and kept nowhere: the store's
/// own host -- the files' paths, the conversions and the bus's readings -- and the two readings a
/// settings asker is judged by: a guest's row, and which weave is the Terminal this host mounted.
// WL-SETTING-09 -- agents/workshop/settings.md
struct LayoutsOwnerHost {
    std::function<LayoutsHost()> store;
    std::function<scope::GuestRowFacts(loom::WeaveId)> guest_row;
    std::function<loom::WeaveId()> terminal;
};

/// The owner.
class LayoutsOwner
    : public zengine::With<manual::ManualFacet<manual::pages::zengine_layouts::kManual>>::Weave<
          LayoutsOwner, LayoutsOwnerState,
          loom::Accept<LayoutEditRequested, SessionSaveRequested, SetupApplyRequested,
                       PaneSettingsDeclared, v2::PaneSettingsDeclared, RoleSettingsDeclared,
                       PaneSettingsAsked, PaneSettingUnused, PaneSettingsRequested,
                       PaneSettingCommitRequested, RoleSettingsRequested,
                       RoleSettingCommitRequested, PaneSettingPinRequested,
                       PaneSettingLockRequested, LayoutSettingsRequested, SettingsWriteRequested,
                       PaneInventory, WeaversActAnswered, PaneCloseRequested,
                       // ...and as a pane built on an earlier vocabulary says them
                       v1::PaneCloseRequested, OfficeSettingsDeclared, OfficeSettingsRequested,
                       OfficeSettingCommitRequested>,
          loom::Emit<LiveLayout, LayoutEditAnswered, SessionSaveAnswered, loom::Ack, loom::Refused,
                     PaneSettings, v2::PaneSettings, PaneSettingsAnswered,
                     PaneSettingCommitAnswered, RoleSettings, RoleSettingsAnswered,
                     RoleSettingCommitAnswered, PaneSettingPinAnswered, PaneSettingLockAnswered,
                     LayoutSettingsAnswered, SettingsWriteAnswered, WeaversActRequested,
                     PaneInventoryRequested, PaneCloseAnswered, v2::PaneCloseAnswered,
                     OfficeSettingsAnswered, OfficeSettingCommitAnswered>> {
public:
    explicit LayoutsOwner(LayoutsOwnerHost host) : host_(std::move(host)), store_(readings_) {}

    // ---- The presenter's link ---------------------------------------------------------------
    void on(const LayoutEditRequested& asked, loom::Mail& mail);
    void on(const SessionSaveRequested& asked, loom::Mail& mail);
    void on(const PaneInventory& said, loom::Mail& mail);
    void on(const WeaversActAnswered& answered, loom::Mail& mail);

    // ---- A desk put live from a caller's text ----------------------------------------------
    void on(const SetupApplyRequested& asked, loom::Mail& mail);

    // ---- A pane's row taken off the live layout ---------------------------------------------
    void on(const PaneCloseRequested& asked, loom::Mail& mail);
    void on(const v1::PaneCloseRequested& asked, loom::Mail& mail);
    PaneCloseAnswered close_asked(const PaneCloseRequested& asked, loom::Mail& mail);

    // ---- What each pane and role takes ----------------------------------------------------
    void on(const PaneSettingsDeclared& declared, loom::Mail& mail);
    void on(const v2::PaneSettingsDeclared& declared, loom::Mail& mail);
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
    /// THE HOST'S READINGS, taken now: for a rig that reads the store between messages.
    void read_host_now() { read_host(); }

private:
    struct PendingUnlock {
        std::uint64_t question = 0;
        loom::DeferredAnswer answer;
        PaneSettingLockRequested asked;
        std::string refused;
    };

    std::string asker_refused(const loom::Mail& mail) const;
    bool from_terminal(const loom::Mail& mail) const;
    bool from_presenter(const loom::Mail& mail) const;
    void send_live(loom::Mail& mail, const StoreEffects& fx);
    void hand_rows(loom::Mail& mail);

    /// THE HOST'S READINGS, read afresh before each ask is judged.
    void read_host();

    LayoutsOwnerHost host_;
    LayoutsHost readings_;
    LayoutStore store_;
    std::vector<PendingUnlock> unlocks_;
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
