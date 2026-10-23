// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The quick setup: a weaver's first start asks how they edit text and shows where the
// ready-to-use setups are, once. It keeps one office-wide setting in Workshop's settings file,
// `answered`, and shows itself only at a start that keeps one; every answer goes through the
// owner of what it changes -- the editor switch, Workshop's settings -- and is said in its words.
// Reference: workshop/docs/zengine.first-run.md; law: agents/workshop/first-run.md.

#include "workshop/first-run/setup.hpp"

#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_drawing.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/settings_seam_vocabulary.hpp"

#include "activation/activation.hpp"
#include "graphics/role.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

namespace ws = zengine::workshop;
namespace gfx = zengine::graphics;
namespace input = zengine::input;
namespace fr = zengine::first_run;

constexpr const char* kWorkshop = "zengine.workshop";
constexpr const char* kOffice = fr::kFirstRunRole;
constexpr const char* kPane = fr::kSetupPane;

/// WHETHER THIS WEAVE ASKED FOR ITS PANE AT A START: kept across a reload in place, so a reload
/// never shows the quick setup a second time in one run.
struct FirstRunState {
    bool launched = false;
    ZEN_SHAPE(FirstRunState, 1, ZEN_FIELD(launched));
};

/// Whether Workshop keeps a settings file this run, as its `written` says: an isolated run, a
/// file refused at launch, or settings withheld keep none, and the setup shows nothing by itself.
bool keeps_settings(const std::string& written) {
    for (const char* none : {"isolated run", "refused at launch", "settings withheld"}) {
        if (written.rfind(none, 0) == 0) {
            return false;
        }
    }
    return true;
}

// WL-FIRST-01 -- agents/workshop/first-run.md
class FirstRun
    : public loom::WeaveBase<
          FirstRun, FirstRunState,
          loom::Accept<loom::Activated, ws::PaneCatalogRequested, ws::PaneRoom,
                       ws::PaneActionRequested, ws::OfficeSettings, ws::OfficeSettingsShown,
                       ws::PaneInventory,
                       ws::PaneSettingActed, ws::PaneLaunchAnswered, ws::PaneCloseAnswered,
                       ws::EditorSwitchAnswered, gfx::AreaRoom, gfx::DrawnRefused,
                       gfx::AreaButtonChanged>,
          loom::Emit<ws::v2::PaneOffered, ws::v3::PaneActions, ws::OfficeSettingsDeclared,
                     ws::OfficeSettingsRequested, ws::OfficeSettingCommitRequested,
                     ws::PaneLaunchRequested, ws::PaneCloseRequested, ws::PaneInventoryRequested,
                     ws::EditorSwitchRequested,
                     ws::EditorSwitchConfirmed, ws::EditorSwitchStatusRequested, gfx::Drawn,
                     gfx::RoomAsked>> {
public:
    /// EVERY ACTIVATION: the pane offered and its rows declared, its one setting declared and
    /// read, the editor's choice asked, and the area's room asked while no window has said it.
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        offer(mail);
        declare(mail);
        settings_ask_ = ++asks_;
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, ws::OfficeSettingsRequested{kOffice},
                                                 settings_ask_);
        ask_status(mail);
        if (!window_.shown()) {
            (void)mail.as_role(kOffice).send_to_role(gfx::kGraphicsRole,
                                                     gfx::RoomAsked{window_.area()});
        }
    }
    void on(const ws::PaneCatalogRequested&, loom::Mail& mail) {
        if (mail.authored_from_role(kWorkshop)) {
            offer(mail);
        }
    }

    /// THE PANE OPENED: the editor's choice asked again, since a Terminal may have switched it.
    void on(const ws::PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || room.pane != kPane) {
            return;
        }
        ask_status(mail);
        show(mail);
    }

    /// ITS SETTING AS WORKSHOP KEEPS IT -- and, at a start that keeps a settings file and has not
    /// answered, the quick setup shown, once a run.
    // WL-FIRST-01 -- agents/workshop/first-run.md
    void on(const ws::OfficeSettingsShown& shown, loom::Mail& mail) {
        if (!answers(mail, settings_ask_) || shown.office != kOffice) {
            return;
        }
        read(shown.revision, shown.settings);
        if (state_.launched || answered_ || !keeps_settings(shown.written)) {
            return;
        }
        // ...ON A FRESH DESK ALONE: the inventory is asked first, and a pane a hand already opened
        // keeps the keys.
        state_.launched = true;
        inventory_ask_ = ++asks_;
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, ws::PaneInventoryRequested{},
                                                 inventory_ask_);
    }
    // WL-FIRST-01 -- agents/workshop/first-run.md
    void on(const ws::PaneInventory& inventory, loom::Mail& mail) {
        if (!answers(mail, inventory_ask_)) {
            return;
        }
        inventory_ask_ = 0;
        std::vector<std::pair<std::string, std::string>> open;
        for (const ws::InventoryPane& p : inventory.panes) {
            if (p.open) {
                open.emplace_back(p.office, p.pane);
            }
        }
        if (!fr::fresh_desk(open)) {
            return; // not shown by itself this run; the Pane Manager still lists it
        }
        launch_ask_ = ++asks_;
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, ws::PaneLaunchRequested{kOffice, kPane},
                                                 launch_ask_);
    }
    void on(const ws::OfficeSettings& kept, loom::Mail& mail) {
        if (mail.authored_from_role(kWorkshop)) {
            read(kept.revision, kept.settings);
        }
    }
    void on(const ws::PaneLaunchAnswered& answer, loom::Mail& mail) {
        if (answers(mail, launch_ask_) && !answer.refusal.empty()) {
            moment_.notice = answer.refusal;
        }
    }
    void on(const ws::PaneCloseAnswered&, loom::Mail&) {}

    /// DONE OR SKIP, AS WORKSHOP ANSWERED THE WRITE: the pane put down either way, so a settings
    /// file that would not take it never holds the weaver at this pane.
    void on(const ws::PaneSettingActed& acted, loom::Mail& mail) {
        if (!committing_ || !answers(mail, commit_)) {
            return;
        }
        committing_ = false;
        if (acted.accepted) {
            answered_ = true;
            revision_ = acted.revision;
            moment_.notice.clear();
        } else {
            moment_.notice = "not kept: " + acted.refusal;
        }
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, ws::PaneCloseRequested{kOffice, kPane},
                                                 ++asks_);
        show(mail);
    }

    /// THE EDITOR SWITCH'S WORD, in its own words: the box follows what holds the office.
    // WL-FIRST-03 -- agents/workshop/first-run.md
    void on(const ws::EditorSwitchAnswered& a, loom::Mail& mail) {
        namespace out = ws::switch_outcome;
        if (answers(mail, status_ask_)) {
            status_ask_ = 0;
        } else if (moment_.switching && answers(mail, switch_ask_)) {
            switch_ask_ = 0;
        } else {
            return;
        }
        if (a.outcome == out::kStatus) {
            if (!moment_.switching) {
                moment_.neovim = a.active == fr::kNeovimChoice;
            }
            show(mail);
            return;
        }
        moment_.switching = false;
        moment_.consent_asked = false;
        consent_.clear();
        if (a.outcome == out::kSwitched || a.outcome == out::kAlreadyActive) {
            moment_.neovim = a.destination == fr::kNeovimChoice;
            moment_.switch_said = moment_.neovim ? "Neovim edits text now"
                                                 : "the standard Editor edits text now";
        } else if (a.outcome == out::kNeedsConfirmation) {
            op_ = a.op;
            consent_ = a.consent;
            moment_.consent_asked = true;
            std::string losses;
            for (const std::string& l : a.losses) {
                losses += (losses.empty() ? "" : "; ") + l;
            }
            moment_.switch_said = "switching would lose: " + losses + " -- choose again to switch";
        } else {
            moment_.switch_said = a.outcome + (a.detail.empty() ? "" : ": " + a.detail);
            ask_status(mail);
        }
        show(mail);
    }

    /// ITS ROWS, as Workshop sent them for a hand's key.
    void on(const ws::PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || asked.pane != kPane) {
            return;
        }
        if (asked.id == fr::kActionUp || asked.id == fr::kActionDown) {
            moment_.cursor = fr::stepped(moment_.cursor, asked.id == fr::kActionUp ? -1 : 1);
            show(mail);
        } else if (asked.id == fr::kActionChoose) {
            choose(mail);
        }
    }

    /// THE AREA'S ROOM, as the window that shows it says it: drawn whole there.
    void on(const gfx::AreaRoom& r, loom::Mail& mail) {
        if (window_.room(r, mail)) {
            show(mail);
        }
    }
    void on(const gfx::DrawnRefused& refused, loom::Mail& mail) {
        (void)window_.refused(refused, mail);
    }

    /// A PRESS ON AN ITEM'S LINE chooses it, as the choose row would with the cursor there.
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        const std::vector<fr::Line> lines = fr::lines_of(moment_);
        const auto p = window_.pointer(b, mail, rows(), columns());
        if (!p || !b.pressed || b.button != 1 || !p->at.shown || p->at.row < 0 ||
            static_cast<std::size_t>(p->at.row) >= lines.size()) {
            return;
        }
        const fr::Item item = lines[static_cast<std::size_t>(p->at.row)].item;
        if (item != fr::Item::kNone) {
            moment_.cursor = item;
            choose(mail);
        }
    }

private:
    void offer(loom::Mail& mail) {
        (void)mail.as_role(kOffice).send_to_role(
            kWorkshop, ws::v2::PaneOffered{kPane, "Quick setup",
                                           "how you edit text, and where the setups are", 14, 72});
        ws::v3::PaneActions actions;
        actions.pane = kPane;
        actions.rows.push_back(ws::v3::PaneActionRow{fr::kActionUp, "previous item",
                                                     input::scan::kUp, input::mod::kNone, "", {}});
        actions.rows.push_back(ws::v3::PaneActionRow{fr::kActionDown, "next item",
                                                     input::scan::kDown, input::mod::kNone, "", {}});
        actions.rows.push_back(ws::v3::PaneActionRow{
            fr::kActionChoose, "choose the item", input::scan::kReturn, input::mod::kNone, "",
            {ws::cause_of<ws::EditorSwitchRequested>(), ws::cause_of<ws::EditorSwitchConfirmed>(),
             ws::cause_of<ws::OfficeSettingCommitRequested>(),
             ws::cause_of<ws::PaneCloseRequested>()}});
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, actions);
    }

    /// ITS ONE SETTING, office-wide: `answered`, off by default.
    void declare(loom::Mail& mail) {
        ws::v2::PaneSettingRow answered;
        answered.key = fr::kAnsweredKey;
        answered.flag = false;
        answered.tags = {"chrome"};
        answered.summary = "the quick setup was answered or skipped"; // at most 64 characters
        (void)mail.as_role(kOffice).send_to_role(kWorkshop,
                                                 ws::OfficeSettingsDeclared{{answered}});
    }

    void ask_status(loom::Mail& mail) {
        status_ask_ = ++asks_;
        (void)mail.as_role(kOffice).send_to_role(ws::kEditorSwitchRole,
                                                 ws::EditorSwitchStatusRequested{}, status_ask_);
    }

    /// Whether `mail` answers the ask that went out under `asked`, this weave's own number.
    static bool answers(const loom::Mail& mail, std::uint64_t asked) {
        return asked != 0 && mail.answers_ask() && mail.correlation() == asked;
    }

    void read(std::int64_t revision, const std::vector<ws::PaneSetting>& settings) {
        revision_ = revision;
        answered_ = false;
        for (const ws::PaneSetting& s : settings) {
            if (s.key == fr::kAnsweredKey) {
                answered_ = s.flag.value_or(false);
            }
        }
    }

    /// WHAT IS UNDER THE CURSOR, DONE: the box asks the switch (or confirms the consent it asked);
    /// Done and Skip write `answered` and put the pane down when Workshop has answered.
    // WL-FIRST-02 -- agents/workshop/first-run.md
    void choose(loom::Mail& mail) {
        if (moment_.cursor == fr::Item::kNeovim) {
            if (moment_.switching) {
                moment_.notice = "the editor switch is still under way";
            } else if (!consent_.empty()) {
                moment_.notice.clear();
                moment_.switching = send_switch(
                    mail, ws::EditorSwitchConfirmed{op_, consent_});
            } else {
                moment_.notice.clear();
                moment_.switching = send_switch(
                    mail, ws::EditorSwitchRequested{moment_.neovim ? fr::kStandardChoice
                                                                   : fr::kNeovimChoice});
            }
            show(mail);
            return;
        }
        if (committing_) {
            return;
        }
        ws::OfficeSettingCommitRequested commit;
        commit.office = kOffice;
        commit.revision = revision_;
        commit.key = fr::kAnsweredKey;
        commit.flag = true;
        const std::uint64_t asked = ++asks_;
        if (!mail.as_role(kOffice).send_to_role(kWorkshop, commit, asked).valid()) {
            moment_.notice = "Workshop could not be asked to keep the answer";
            show(mail);
            return;
        }
        committing_ = true;
        commit_ = asked;
    }

    template <class Ask>
    bool send_switch(loom::Mail& mail, const Ask& ask) {
        const std::uint64_t asked = ++asks_;
        if (!mail.as_role(kOffice).send_to_role(ws::kEditorSwitchRole, ask, asked).valid()) {
            moment_.notice = "the editor switch could not be asked";
            return false;
        }
        switch_ask_ = asked;
        moment_.switch_said.clear();
        return true;
    }

    std::int64_t rows() const { return window_.rows_room().rows(); }
    std::int64_t columns() const { return window_.rows_room().columns(); }

    /// THE QUICK SETUP DRAWN in its area, line for line as `lines_of` says it.
    void show(loom::Mail& mail) {
        if (!window_.shown() || rows() <= 0 || columns() <= 0) {
            return;
        }
        std::vector<zengine::surface::SurfaceTextRow> drawn;
        for (const fr::Line& l : fr::lines_of(moment_)) {
            std::int64_t role = gfx::role::kFill;
            switch (l.ink) {
            case fr::Line::Ink::kHeading:
            case fr::Line::Ink::kCursor: role = gfx::role::kAccent; break;
            case fr::Line::Ink::kMuted: role = gfx::role::kMuted; break;
            case fr::Line::Ink::kAlert: role = gfx::role::kAlert; break;
            case fr::Line::Ink::kPlain: break;
            }
            drawn.push_back({l.text, role});
        }
        const ws::RowsDrawing rows_drawn =
            ws::rows_drawing(drawn, std::vector<ws::PaneRowPart>{}, window_.rows_room(),
                             columns(), ws::RowsCaret{}, rows());
        window_.draw(rows_drawn.sketch, mail);
    }

    // NOT STATE: Workshop keeps the setting and the switch keeps the editor; these are read again
    // at every activation.
    fr::Moment moment_;
    bool answered_ = false;
    std::int64_t revision_ = 0;
    bool committing_ = false;
    /// THIS WEAVE'S OWN NUMBERS, one an ask, and the ask each kind of answer is read against.
    std::uint64_t asks_ = 0, settings_ask_ = 0, inventory_ask_ = 0, launch_ask_ = 0, commit_ = 0,
                  status_ask_ = 0, switch_ask_ = 0;
    std::int64_t op_ = 0;
    std::string consent_;
    ws::WindowDrawing window_{kOffice, kOffice};
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(FirstRun)
