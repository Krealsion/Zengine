// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The quick setup and "Start here": how a weaver edits text and the setups to start, asked once;
// then the story as a checklist, shown at each start until done. It keeps `answered` and `story`
// with the layouts' owner, and shows itself only at a start that keeps a settings file. Every answer
// goes to the owner of what it changes, and every tick is a fact its owner said.
// Reference: workshop/docs/zengine.first-run.md; law: agents/workshop/first-run.md.

#include "workshop/first-run/setup.hpp"
#include "workshop/first-run/story.hpp"

#include "workshop/actor_scope.hpp"
#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_drawing.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/settings_seam_vocabulary.hpp"

#include "activation/activation.hpp"
#include "graphics/role.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "launcher/vocabulary.hpp"
#include "view/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace {

namespace ws = zengine::workshop;
namespace gfx = zengine::graphics;
namespace input = zengine::input;
namespace fr = zengine::first_run;
namespace launcher = zengine::launcher;
namespace view = zengine::view;

constexpr const char* kWorkshop = "zengine.workshop";
constexpr const char* kOffice = fr::kFirstRunRole;
constexpr const char* kPane = fr::kSetupPane;
constexpr const char* kStory = fr::kStoryPane;

/// WHAT THIS WEAVE ASKED FOR AT A START: kept across a reload in place, so a reload never shows
/// either pane by itself a second time in one run.
struct FirstRunState {
    bool launched = false;
    bool story_shown = false;
    ZEN_SHAPE(FirstRunState, 2, ZEN_FIELD(launched), ZEN_FIELD(story_shown));
};

/// Whether a settings file is kept this run, as the layouts' owner's `written` says: an isolated run, a
/// file refused at launch, or settings withheld keep none, and the setup shows nothing by itself.
bool keeps_settings(const std::string& written) {
    for (const char* none : {"isolated run", "refused at launch", "settings withheld"}) {
        if (written.rfind(none, 0) == 0) {
            return false;
        }
    }
    return true;
}

/// A step's number in its owner's words: index 0 is step 1.
constexpr std::size_t kMakeView = 0, kWireButton = 1, kShowAnswer = 2, kMakeApp = 3, kStartApp = 4;

// WL-FIRST-01, WL-FIRST-04 -- agents/workshop/first-run.md
class FirstRun
    : public loom::WeaveBase<
          FirstRun, FirstRunState,
          loom::Accept<loom::Activated, ws::PaneCatalogRequested,
                       ws::PaneActionRequested, ws::OfficeSettings, ws::OfficeSettingsAnswered,
                       ws::PaneInventory, ws::OfficeSettingCommitAnswered, ws::PaneLaunchAnswered,
                       ws::PaneCloseAnswered, ws::PaneOperationAnswered, ws::EditorSwitchAnswered,
                       launcher::LaunchCatalog, launcher::Launched, view::ViewWiring,
                       loom::DispatchRefused, gfx::AreaRoom, gfx::DrawnRefused,
                       gfx::AreaButtonChanged>,
          loom::Emit<ws::v2::PaneOffered, ws::v3::PaneActions, ws::OfficeSettingsDeclared,
                     ws::OfficeSettingsRequested, ws::OfficeSettingCommitRequested,
                     ws::PaneLaunchRequested, ws::v2::PaneLaunchRequested, ws::PaneCloseRequested,
                     ws::PaneInventoryRequested, ws::v2::PaneOperationRequested,
                     ws::EditorSwitchRequested, ws::EditorSwitchConfirmed,
                     ws::EditorSwitchQuietStatusRequested, launcher::LaunchCatalogRequested,
                     launcher::Launch, view::ViewWiringRequested, gfx::Drawn, gfx::RoomAsked>> {
public:
    /// EVERY ACTIVATION: both panes offered and their rows declared, the two settings declared and
    /// read, the editor's choice asked, what can tick a step asked of its owners, and each area's
    /// room asked while no window has said it.
    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        offer(mail);
        declare(mail);
        settings_ask_ = ++asks_;
        (void)mail.as_role(kOffice).send_to_role(ws::kLayoutsRole,
                                                 ws::OfficeSettingsRequested{kOffice},
                                                 settings_ask_);
        ask_status(mail);
        ask_facts(mail);
        for (const ws::WindowDrawing* w : {&window_, &story_window_}) {
            if (!w->shown()) {
                (void)mail.as_role(kOffice).send_to_role(gfx::kGraphicsRole, gfx::RoomAsked{w->area()});
            }
        }
    }
    void on(const ws::PaneCatalogRequested&, loom::Mail& mail) {
        if (mail.authored_from_role(kWorkshop)) {
            offer(mail);
        }
    }

    /// ITS SETTINGS AS THE LAYOUTS' OWNER KEEPS THEM -- and, at a start that keeps a settings file,
    /// the quick setup shown once a run while it is not answered, or "Start here" seated beside
    /// what holds the keys while the story is not done.
    // WL-FIRST-01, WL-FIRST-05 -- agents/workshop/first-run.md
    void on(const ws::OfficeSettingsAnswered& shown, loom::Mail& mail) {
        if (!answers(mail, settings_ask_) || shown.office != kOffice) {
            return;
        }
        read(shown.revision, shown.settings, mail);
        if (!keeps_settings(shown.written)) {
            return;
        }
        if (answered_) {
            if (!state_.story_shown && !fr::story_done(ticks_)) {
                state_.story_shown = true;
                launch_story(mail, /*keys=*/false);
            }
            return;
        }
        if (state_.launched) {
            return;
        }
        // ...ON A FRESH DESK ALONE: the inventory is asked first, and a pane a hand already opened
        // keeps the keys.
        state_.launched = true;
        fresh_ask_ = ++asks_;
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, ws::PaneInventoryRequested{}, fresh_ask_);
    }
    void on(const ws::OfficeSettings& kept, loom::Mail& mail) {
        if (mail.authored_from_role(ws::kLayoutsRole)) {
            read(kept.revision, kept.settings, mail);
        }
    }

    /// THE INVENTORY, answered to this weave's ask or said to any: whether the desk is fresh for
    /// the quick setup, and whether a running view is on it (step 1). A view newly there is asked
    /// what its wiring has done.
    // WL-FIRST-01, WL-FIRST-04 -- agents/workshop/first-run.md
    void on(const ws::PaneInventory& inventory, loom::Mail& mail) {
        const bool fresh = answers(mail, fresh_ask_);
        if (!fresh && !answers(mail, inventory_ask_) && !mail.authored_from_role(kWorkshop)) {
            return; // Workshop's answer to this weave's ask, or what it said to any
        }
        std::set<std::string> views;
        std::vector<std::pair<std::string, std::string>> open;
        for (const ws::InventoryPane& p : inventory.panes) {
            if (p.open) {
                open.emplace_back(p.office, p.pane);
            }
            if (p.pane == view::kPane && p.available) {
                views.insert(p.office);
            }
        }
        const bool new_view = std::any_of(views.begin(), views.end(), [&](const std::string& v) {
            return view_offices_.count(v) == 0;
        });
        view_offices_ = views;
        if (!views.empty()) {
            fact(kMakeView, mail);
        }
        if (new_view) {
            (void)mail.as_role(kOffice).publish(view::ViewWiringRequested{});
        }
        ask_listing(mail);
        if (!fresh) {
            return;
        }
        fresh_ask_ = 0;
        if (!fr::fresh_desk(open)) {
            return; // not shown by itself this run; the Pane Manager still lists it
        }
        launch_ask_ = ++asks_;
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, ws::PaneLaunchRequested{kOffice, kPane},
                                                 launch_ask_);
    }

    /// WHAT A RUNNING VIEW'S WIRING HAS DONE, said by the view as its own office: an intent
    /// something accepted ticks step 2, a field it was told step 3.
    // WL-FIRST-04 -- agents/workshop/first-run.md
    void on(const view::ViewWiring& wiring, loom::Mail& mail) {
        if (view_offices_.count(std::string(mail.authored_role())) == 0) {
            return; // only a running view on the desk says its own wiring
        }
        if (std::any_of(wiring.intents.begin(), wiring.intents.end(),
                        [](const view::ViewIntentWiring& i) { return i.sent && i.accepted; })) {
            fact(kWireButton, mail);
        }
        if (std::any_of(wiring.fields.begin(), wiring.fields.end(),
                        [](const view::ViewFieldWiring& f) { return f.told; })) {
            fact(kShowAnswer, mail);
        }
    }

    /// THE LAUNCHER'S LISTING, the answer to this weave's own ask: the setups the quick setup
    /// lists, an app in the project's apps/ (step 4), and an app that runs (step 5).
    // WL-FIRST-04 -- agents/workshop/first-run.md
    void on(const launcher::LaunchCatalog& listed, loom::Mail& mail) {
        if (!answers(mail, listing_ask_)) {
            return;
        }
        listing_ask_ = 0;
        moment_.setups.clear();
        for (const launcher::LaunchEntry& e : listed.entries) {
            if (e.kind == launcher::kKindDemo) {
                moment_.setups.push_back(fr::SetupRow{e.name, e.title, e.line});
            } else if (e.kind == launcher::kKindApp) {
                fact(kMakeApp, mail);
                if (e.running) {
                    fact(kStartApp, mail);
                }
            }
        }
        moment_.setups_note = moment_.setups.empty()
                                  ? "this Workshop starts no setup: it was configured with no demo "
                                    "service"
                                  : std::string();
        const std::vector<fr::Item> items = fr::items_of(moment_);
        if (std::find(items.begin(), items.end(), moment_.cursor) == items.end()) {
            moment_.cursor = fr::Item::kStory; // the setup it stood on is no longer listed
        }
        show(mail);
    }

    /// A START, as the launcher answered this weave's own or said it to any: an app started ticks
    /// step 5; the quick setup's own start is said under its setups.
    // WL-FIRST-04 -- agents/workshop/first-run.md
    void on(const launcher::Launched& said, loom::Mail& mail) {
        if (mail.answers_ask()) {
            if (!answers(mail, start_ask_)) {
                return;
            }
            start_ask_ = 0;
            moment_.notice = said.ok ? "started the setup `" + said.name + "`, process " +
                                           std::to_string(said.pid) + ": it goes on without Workshop"
                                     : "nothing was started -- " + said.reason;
            show(mail);
            return;
        }
        if (said.ok && said.kind == launcher::kKindApp &&
            mail.authored_from_role(launcher::kLauncherRole)) {
            fact(kMakeApp, mail);
            fact(kStartApp, mail);
        }
    }

    /// LOOM'S WORD THAT THE LISTING COULD NOT BE ASKED: no launcher answers in this Workshop.
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!mail.dispatch_refused() || refused.role != launcher::kLauncherRole) {
            return;
        }
        if (refused.shape == launcher::LaunchCatalogRequested::zen_name) {
            listing_ask_ = 0;
            moment_.setups.clear();
            moment_.setups_note = "no launcher answers in this Workshop (" + refused.reason + ")";
        } else if (refused.shape == launcher::Launch::zen_name) {
            start_ask_ = 0;
            moment_.notice = "nothing was started -- the launcher could not be asked (" +
                             refused.reason + ")";
        }
        show(mail);
    }

    void on(const ws::PaneLaunchAnswered& answer, loom::Mail& mail) {
        if (answers(mail, launch_ask_)) {
            launch_ask_ = 0;
            if (!answer.refusal.empty()) {
                moment_.notice = answer.refusal;
            }
        } else if (answers(mail, story_launch_ask_)) {
            story_launch_ask_ = 0;
            if (!answer.refusal.empty()) {
                story_.notice = answer.refusal;
                show_story(mail);
            }
        } else if (answers(mail, show_me_ask_)) {
            show_me_ask_ = 0;
            story_.notice = answer.refusal.empty()
                                ? "Show me: " + std::string(fr::steps()[show_me_step_].move)
                                : answer.refusal;
            show_story(mail);
        }
    }
    void on(const ws::PaneCloseAnswered&, loom::Mail&) {}

    /// A SETTING WRITTEN, as the layouts' owner answered it: Done and Skip put the quick setup
    /// down and open "Start here" either way, so a settings file that would not take it never
    /// holds the weaver at this pane; a story's write that did not land is said in its pane.
    void on(const ws::OfficeSettingCommitAnswered& acted, loom::Mail& mail) {
        if (commit_ask_ == 0 || !answers(mail, commit_ask_)) {
            return;
        }
        commit_ask_ = 0;
        if (acted.accepted) {
            revision_ = acted.revision;
        }
        if (committing_ == Commit::kAnswered) {
            committing_ = Commit::kNone;
            if (acted.accepted) {
                answered_ = true;
                moment_.notice.clear();
            } else {
                moment_.notice = "not kept: " + acted.refusal;
            }
            (void)mail.as_role(kOffice).send_to_role(ws::kLayoutsRole,
                                                     ws::PaneCloseRequested{kOffice, kPane}, ++asks_);
            show(mail);
            if (!fr::story_done(ticks_)) {
                state_.story_shown = true;
                launch_story(mail, /*keys=*/true);
            }
        } else {
            committing_ = Commit::kNone;
            if (acted.accepted) {
                kept_ = writing_; // what this write carried; a tick since is written next
            } else {
                story_.notice = "the ticks were not kept: " + acted.refusal;
                show_story(mail);
            }
        }
        flush(mail);
    }

    /// WORKSHOP'S WORD ON A START THE QUICK SETUP ASKED ABOUT, judged for the hand that pressed:
    /// allowed, the launcher is asked.
    void on(const ws::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (!answers(mail, approval_ask_)) {
            return;
        }
        approval_ask_ = 0;
        if (!answer.allowed) {
            moment_.notice = "nothing was started -- " + answer.reason;
            starting_.clear();
            show(mail);
            return;
        }
        start_ask_ = ++asks_;
        if (!mail.as_role(kOffice)
                 .send_to_role(launcher::kLauncherRole,
                               launcher::Launch{launcher::kKindDemo, starting_}, start_ask_)
                 .valid()) {
            start_ask_ = 0;
            moment_.notice = "nothing was started -- the launcher could not be asked";
        } else {
            moment_.notice = "starting the setup `" + starting_ + "`";
        }
        starting_.clear();
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

    /// ITS ROWS, as Workshop sent them for a hand's key: the key's number goes with a start, so
    /// Workshop judges it for that hand.
    void on(const ws::PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop)) {
            return;
        }
        if (asked.pane == kPane) {
            if (asked.id == fr::kActionUp || asked.id == fr::kActionDown) {
                moment_.cursor =
                    fr::stepped(moment_, moment_.cursor, asked.id == fr::kActionUp ? -1 : 1);
                show(mail);
            } else if (asked.id == fr::kActionChoose) {
                choose(mail, mail.correlation());
            }
        } else if (asked.pane == kStory) {
            if (asked.id == fr::kStoryUp || asked.id == fr::kStoryDown) {
                story_.cursor = fr::story_stepped(story_.cursor, asked.id == fr::kStoryUp ? -1 : 1);
                show_story(mail);
            } else if (asked.id == fr::kStoryChoose) {
                choose_story(mail);
            }
        }
    }

    /// AN AREA'S ROOM, as the window that shows it says it: drawn whole there.
    void on(const gfx::AreaRoom& r, loom::Mail& mail) {
        if (window_.room(r, mail)) {
            // AN AREA THE WINDOW SHOWS AFRESH is the pane opened: the editor's choice and the
            // setups asked again, since a Terminal may have switched it.
            if (r.empty) {
                ask_status(mail);
                ask_listing(mail);
            }
            show(mail);
        } else if (story_window_.room(r, mail)) {
            if (r.empty) {
                ask_facts(mail);
            }
            show_story(mail);
        }
    }
    void on(const gfx::DrawnRefused& refused, loom::Mail& mail) {
        if (!window_.refused(refused, mail)) {
            (void)story_window_.refused(refused, mail);
        }
    }

    /// A PRESS ON AN ITEM'S LINE chooses it, as the choose row would with the cursor there.
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        if (const auto p = window_.pointer(b, mail, rows(window_), columns(window_))) {
            pressed_setup(*p, b, mail);
        } else if (const auto s = story_window_.pointer(b, mail, rows(story_window_),
                                                        columns(story_window_))) {
            pressed_story(*s, b, mail);
        }
    }

private:
    enum class Commit { kNone, kAnswered, kTicks };

    void offer(loom::Mail& mail) {
        (void)mail.as_role(kOffice).send_to_role(
            kWorkshop, ws::v2::PaneOffered{kPane, "Quick setup",
                                           "how you edit text, and the ready-to-use setups", 22, 80});
        (void)mail.as_role(kOffice).send_to_role(
            kWorkshop, ws::v2::PaneOffered{kStory, "Start here",
                                           "the story a step at a time, each ticked as it happens",
                                           24, 80});
        const auto rows = [](const char* pane, const char* up, const char* down, const char* choose,
                             std::vector<ws::ActionCause> causes) {
            ws::v3::PaneActions actions;
            actions.pane = pane;
            actions.rows.push_back(ws::v3::PaneActionRow{up, "previous item", input::scan::kUp,
                                                         input::mod::kNone, "", {}});
            actions.rows.push_back(ws::v3::PaneActionRow{down, "next item", input::scan::kDown,
                                                         input::mod::kNone, "", {}});
            actions.rows.push_back(ws::v3::PaneActionRow{choose, "choose the item",
                                                         input::scan::kReturn, input::mod::kNone,
                                                         "", std::move(causes)});
            return actions;
        };
        (void)mail.as_role(kOffice).send_to_role(
            kWorkshop,
            rows(kPane, fr::kActionUp, fr::kActionDown, fr::kActionChoose,
                 {ws::cause_of<ws::EditorSwitchRequested>(), ws::cause_of<ws::EditorSwitchConfirmed>(),
                  ws::cause_of<ws::OfficeSettingCommitRequested>(),
                  ws::cause_of<ws::PaneCloseRequested>(), ws::cause_of<launcher::Launch>(),
                  ws::cause_of<ws::PaneLaunchRequested>()}));
        (void)mail.as_role(kOffice).send_to_role(
            kWorkshop, rows(kStory, fr::kStoryUp, fr::kStoryDown, fr::kStoryChoose,
                            {ws::cause_of<ws::OfficeSettingCommitRequested>(),
                             ws::cause_of<ws::PaneLaunchRequested>()}));
    }

    /// ITS TWO SETTINGS, office-wide: `answered`, off by default, and `story`, no step ticked.
    void declare(loom::Mail& mail) {
        ws::v2::PaneSettingRow answered;
        answered.key = fr::kAnsweredKey;
        answered.flag = false;
        answered.tags = {"chrome"};
        answered.summary = "the quick setup was answered or skipped"; // at most 64 characters
        ws::v2::PaneSettingRow story;
        story.key = fr::kStoryKey;
        story.text = std::string();
        story.tags = {"chrome"};
        story.summary = "the story's steps ticked, h for one you ticked"; // at most 64 characters
        (void)mail.as_role(kOffice).send_to_role(ws::kLayoutsRole,
                                                 ws::OfficeSettingsDeclared{{answered, story}});
    }

    /// THE SWITCH'S STATUS, ASKED QUIETLY: the box shows the answer, so nothing is said of it.
    void ask_status(loom::Mail& mail) {
        status_ask_ = ++asks_;
        (void)mail.as_role(kOffice).send_to_role(ws::kEditorSwitchRole,
                                                 ws::EditorSwitchQuietStatusRequested{}, status_ask_);
    }

    /// WHAT CAN TICK A STEP, asked of each owner: the inventory, every running view's wiring, the
    /// launcher's listing.
    void ask_facts(loom::Mail& mail) {
        inventory_ask_ = ++asks_;
        (void)mail.as_role(kOffice).send_to_role(kWorkshop, ws::PaneInventoryRequested{},
                                                 inventory_ask_);
        (void)mail.as_role(kOffice).publish(view::ViewWiringRequested{});
        ask_listing(mail);
    }
    void ask_listing(loom::Mail& mail) {
        if (listing_ask_ != 0) {
            return; // one listing in flight
        }
        listing_ask_ = ++asks_;
        if (!mail.as_role(kOffice)
                 .send_to_role(launcher::kLauncherRole, launcher::LaunchCatalogRequested{},
                               listing_ask_)
                 .valid()) {
            listing_ask_ = 0;
            moment_.setups_note = "no launcher answers in this Workshop";
        }
    }

    /// Whether `mail` answers the ask that went out under `asked`, this weave's own number.
    static bool answers(const loom::Mail& mail, std::uint64_t asked) {
        return asked != 0 && mail.answers_ask() && mail.correlation() == asked;
    }

    /// THE SETTINGS READ: `answered`, and the ticks the file keeps joined to those heard this run
    /// before it was read, written back when they differ.
    void read(std::int64_t revision, const std::vector<ws::PaneSetting>& settings, loom::Mail& mail) {
        revision_ = revision;
        answered_ = false;
        std::string kept;
        for (const ws::PaneSetting& s : settings) {
            if (s.key == fr::kAnsweredKey) {
                answered_ = s.flag.value_or(false);
            } else if (s.key == fr::kStoryKey) {
                kept = s.text.value_or(std::string());
            }
        }
        const fr::Ticks file = fr::ticks_of(kept);
        for (std::size_t i = 0; i < fr::kSteps; ++i) {
            if (ticks_[i] != fr::Tick::kFact && file[i] != fr::Tick::kNone) {
                ticks_[i] = file[i]; // the file's tick, unless a fact this run already ticked it
            }
        }
        kept_ = kept;
        read_ = true;
        flush(mail);
        show_story(mail);
    }

    /// A STEP'S OWNER SAID IT HAPPENED: ticked as its, a hand's tick of it becoming the fact's.
    void fact(std::size_t step, loom::Mail& mail) {
        if (ticks_[step] == fr::Tick::kFact) {
            return;
        }
        ticks_[step] = fr::Tick::kFact;
        flush(mail);
        show_story(mail);
    }

    /// THE TICKS WRITTEN when they differ from what the file keeps: one write at a time, the next
    /// after its answer, and none before the file was read.
    void flush(loom::Mail& mail) {
        const std::string now = fr::story_text(ticks_);
        if (!read_ || committing_ != Commit::kNone || now == kept_) {
            return;
        }
        ws::OfficeSettingCommitRequested commit;
        commit.office = kOffice;
        commit.revision = revision_;
        commit.key = fr::kStoryKey;
        commit.text = now;
        writing_ = now;
        commit_ask_ = ++asks_;
        if (!mail.as_role(kOffice).send_to_role(ws::kLayoutsRole, commit, commit_ask_).valid()) {
            commit_ask_ = 0;
            story_.notice = "the layouts' owner could not be asked to keep the ticks";
            return;
        }
        committing_ = Commit::kTicks;
    }

    void launch_story(loom::Mail& mail, bool keys) {
        story_launch_ask_ = ++asks_;
        if (keys) {
            (void)mail.as_role(kOffice).send_to_role(
                kWorkshop, ws::PaneLaunchRequested{kOffice, kStory}, story_launch_ask_);
        } else {
            (void)mail.as_role(kOffice).send_to_role(
                kWorkshop, ws::v2::PaneLaunchRequested{kOffice, kStory, false}, story_launch_ask_);
        }
    }

    /// WHAT IS UNDER THE QUICK SETUP'S CURSOR, DONE: the box asks the switch (or confirms the
    /// consent it asked); a setup's Start asks Workshop for the hand of `gesture`, then the
    /// launcher; The story, Done and Skip write `answered` and put the pane down when the layouts'
    /// owner has answered, opening "Start here".
    // WL-FIRST-02 -- agents/workshop/first-run.md
    void choose(loom::Mail& mail, std::uint64_t gesture) {
        if (moment_.cursor == fr::Item::kNeovim) {
            if (moment_.switching) {
                moment_.notice = "the editor switch is still under way";
            } else if (!consent_.empty()) {
                moment_.notice.clear();
                moment_.switching = send_switch(mail, ws::EditorSwitchConfirmed{op_, consent_});
            } else {
                moment_.notice.clear();
                moment_.switching = send_switch(
                    mail, ws::EditorSwitchRequested{moment_.neovim ? fr::kStandardChoice
                                                                   : fr::kNeovimChoice});
            }
            show(mail);
            return;
        }
        if (const std::int64_t setup = fr::setup_of(moment_.cursor); setup >= 0) {
            start(static_cast<std::size_t>(setup), gesture, mail);
            return;
        }
        if (committing_ == Commit::kAnswered) {
            return;
        }
        if (committing_ != Commit::kNone) {
            moment_.notice = "a setting is still being written -- choose again";
            show(mail);
            return;
        }
        ws::OfficeSettingCommitRequested commit;
        commit.office = kOffice;
        commit.revision = revision_;
        commit.key = fr::kAnsweredKey;
        commit.flag = true;
        const std::uint64_t asked = ++asks_;
        if (!mail.as_role(kOffice).send_to_role(ws::kLayoutsRole, commit, asked).valid()) {
            moment_.notice = "the layouts' owner could not be asked to keep the answer";
            show(mail);
            return;
        }
        committing_ = Commit::kAnswered;
        commit_ask_ = asked;
    }

    /// A SETUP'S START, asked of Workshop first for the hand behind `gesture`, classes `build` and
    /// `write` as Files' start is: one start at a time.
    // WL-FIRST-02 -- agents/workshop/first-run.md
    void start(std::size_t setup, std::uint64_t gesture, loom::Mail& mail) {
        if (setup >= moment_.setups.size()) {
            return;
        }
        if (approval_ask_ != 0 || start_ask_ != 0) {
            moment_.notice = "a start is waiting for its answer; start again once it comes";
            show(mail);
            return;
        }
        ws::v2::PaneOperationRequested asked;
        asked.pane = kPane;
        asked.gesture = static_cast<std::int64_t>(gesture);
        asked.classes = {ws::scope::kBuild, ws::scope::kWrite};
        approval_ask_ = ++asks_;
        if (!mail.as_role(kOffice).send_to_role(kWorkshop, asked, approval_ask_).valid()) {
            approval_ask_ = 0;
            moment_.notice = "nothing was started -- Workshop could not be asked";
            show(mail);
            return;
        }
        starting_ = moment_.setups[setup].name;
    }

    /// WHAT IS UNDER "START HERE"'S CURSOR, DONE: a box ticked or unticked by the weaver's hand
    /// (one its owner ticked stays), or Show me opening the step's pane and saying its next move.
    // WL-FIRST-04 -- agents/workshop/first-run.md
    void choose_story(loom::Mail& mail) {
        const auto step = static_cast<std::size_t>(story_.cursor / 2);
        if (step >= fr::kSteps) {
            return;
        }
        if (story_.cursor % 2 == 1) {
            show_me_step_ = step;
            show_me_ask_ = ++asks_;
            (void)mail.as_role(kOffice).send_to_role(
                kWorkshop,
                ws::PaneLaunchRequested{fr::steps()[step].office, fr::steps()[step].pane},
                show_me_ask_);
            return;
        }
        switch (ticks_[step]) {
        case fr::Tick::kFact:
            story_.notice = std::string("step ") + std::to_string(step + 1) +
                            " was ticked when " + fr::steps()[step].by;
            break;
        case fr::Tick::kHand:
            ticks_[step] = fr::Tick::kNone;
            story_.notice.clear();
            break;
        case fr::Tick::kNone:
            ticks_[step] = fr::Tick::kHand;
            story_.notice.clear();
            break;
        }
        flush(mail);
        show_story(mail);
    }

    void pressed_setup(const ws::WindowPointer& p, const gfx::AreaButtonChanged& b,
                       loom::Mail& mail) {
        const std::vector<fr::Line> lines = fr::lines_of(moment_);
        if (!b.pressed || b.button != 1 || !p.at.shown || p.at.row < 0 ||
            static_cast<std::size_t>(p.at.row) >= lines.size()) {
            return;
        }
        if (p.moved) { // the window landed it on lines that have since moved
            moment_.notice = "the list moved -- press again";
            show(mail);
            return;
        }
        const fr::Item item = lines[static_cast<std::size_t>(p.at.row)].item;
        if (item != fr::Item::kNone) {
            moment_.cursor = item;
            choose(mail, mail.correlation());
        }
    }
    void pressed_story(const ws::WindowPointer& p, const gfx::AreaButtonChanged& b,
                       loom::Mail& mail) {
        const std::vector<fr::StoryLine> lines = fr::story_lines(story_);
        if (!b.pressed || b.button != 1 || !p.at.shown || p.at.row < 0 ||
            static_cast<std::size_t>(p.at.row) >= lines.size()) {
            return;
        }
        if (p.moved) {
            story_.notice = "the list moved -- press again";
            show_story(mail);
            return;
        }
        const std::int64_t item = lines[static_cast<std::size_t>(p.at.row)].item;
        if (item != fr::kNoStoryItem) {
            story_.cursor = item;
            choose_story(mail);
        }
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

    static std::int64_t rows(const ws::WindowDrawing& w) { return w.rows_room().rows(); }
    static std::int64_t columns(const ws::WindowDrawing& w) { return w.rows_room().columns(); }

    /// LINES DRAWN in `w`'s area, each with its role, numbered by what their items mean -- the
    /// line each item stands on -- so a notice under them, which no press chooses, moves nothing.
    template <class Line>
    static void draw(ws::WindowDrawing& w, const std::vector<Line>& lines, loom::Mail& mail) {
        if (!w.shown() || rows(w) <= 0 || columns(w) <= 0) {
            return;
        }
        std::vector<ws::TextRow> drawn;
        std::uint64_t meaning = 1469598103934665603ULL;
        std::uint64_t at = 0;
        for (const Line& l : lines) {
            const auto item = static_cast<std::int64_t>(l.item);
            if (item >= 0) {
                meaning = (meaning ^ ((at << 8) | (static_cast<std::uint64_t>(item) + 1))) *
                          1099511628211ULL;
            }
            ++at;
            std::int64_t role = gfx::role::kFill;
            switch (l.ink) {
            case Line::Ink::kHeading:
            case Line::Ink::kCursor: role = gfx::role::kAccent; break;
            case Line::Ink::kMuted: role = gfx::role::kMuted; break;
            case Line::Ink::kAlert: role = gfx::role::kAlert; break;
            default: break;
            }
            drawn.push_back({l.text, role});
        }
        ws::RowsDrawing rows_drawn = ws::rows_drawing(drawn, std::vector<ws::RowPart>{},
                                                      w.rows_room(), columns(w), ws::RowsCaret{},
                                                      rows(w));
        rows_drawn.sketch.number(static_cast<std::int64_t>(meaning >> 1));
        w.draw(rows_drawn.sketch, mail);
    }
    void show(loom::Mail& mail) { draw(window_, fr::lines_of(moment_), mail); }
    void show_story(loom::Mail& mail) {
        story_.ticks = ticks_;
        draw(story_window_, fr::story_lines(story_), mail);
    }

    // NOT STATE: the layouts' owner keeps the settings, the switch the editor, and each owner its
    // fact; these are read again at every activation.
    fr::Moment moment_;
    fr::StoryMoment story_;
    fr::Ticks ticks_{};
    std::string kept_;    ///< the ticks as the settings file last kept them
    std::string writing_; ///< the ticks the write in flight carries
    bool read_ = false;  ///< the settings were read this activation
    bool answered_ = false;
    std::int64_t revision_ = 0;
    Commit committing_ = Commit::kNone;
    std::set<std::string> view_offices_; ///< the running views the inventory last named
    std::string starting_;               ///< the setup a start waits on Workshop for
    std::size_t show_me_step_ = 0;
    /// THIS WEAVE'S OWN NUMBERS, one an ask, and the ask each kind of answer is read against.
    std::uint64_t asks_ = 0, settings_ask_ = 0, fresh_ask_ = 0, inventory_ask_ = 0, launch_ask_ = 0, commit_ask_ = 0,
                  status_ask_ = 0, switch_ask_ = 0, listing_ask_ = 0, approval_ask_ = 0,
                  start_ask_ = 0, story_launch_ask_ = 0, show_me_ask_ = 0;
    std::int64_t op_ = 0;
    std::string consent_;
    ws::WindowDrawing window_{kOffice, ws::pane_area(kOffice, kPane)};
    ws::WindowDrawing story_window_{kOffice, ws::pane_area(kOffice, kStory)};
    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(FirstRun)
