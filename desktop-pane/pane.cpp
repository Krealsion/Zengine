// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Desktop -- a loadable weave that owns what this application does by default: which
// gestures open or focus which tool, what a key nothing more specific claimed means, what a
// maker reads in the empty room, and what is said about a tool that is not there.
//
// (*) WHAT IT REPLACES, AND WHY EACH PIECE WAS NEVER THE HOST'S. Four things arrive here:
//
//   the `p` picker overlay      a MODE the host owned, which took the keyboard whole and
//                               TOGGLED participation. Launching is a pane now, and a launch
//                               of an open tool focuses it rather than closing it.
//   `workshop.terminal`         a global row in the host's closed catalog. It is an
//                               application row pointed at an ordinary pane now.
//   Escape-to-deselect          a hard-coded line at the end of the host's key handler. It is
//                               a declared row in the default precedence class now, in the
//                               same position, which a maker can move or disable.
//   the object canvas           two rectangles a maker could push around with hjkl, standing
//                               where the desk's own floor belongs.
//
// None of those is a fact about ROOM, FOCUS, REALIZATION or ROOTS -- the four the host keeps
// (VD-19) -- so none of them had to be compiled into it.
//
// (!) WHAT THIS WEAVE CANNOT DO, said here because a desktop is the one participant a reader
// will assume is privileged. It cannot open a pane the host's inventory does not hold; it
// cannot cause an artifact to load; it cannot read another pane's rows; it cannot keep a maker
// from quitting; and it cannot take a gesture from a pane that declared it owns it. Every one
// of those is the host's answer, and asking does not change it (VD-21).

#include "desktop-pane/vocabulary.hpp"

#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace input = zengine::input;
namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace pane = zengine::desktop_pane;

using ws::ActionsRefused;
using ws::AppActionRequested;
using ws::AppActionRow;
using ws::AppActions;
using ws::DeselectRequested;
using ws::DesktopFace;
using ws::InventoryPane;
using ws::PaneActionRequested;
using ws::PaneActionRow;
using ws::PaneActions;
using ws::PaneCatalogRequested;
using ws::PaneContent;
using ws::PaneInventory;
using ws::PaneLaunchAnswered;
using ws::PaneLaunchRequested;
using ws::PaneOffered;
using ws::PaneRoom;

/// WHO THIS WEAVE IS TALKING TO -- the host's office, spelled as a literal exactly as every
/// other provider spells it. A provider is a stranger to Workshop's internals and says who it
/// is talking to the way a third party would.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// THE TOOL THE TERMINAL LAUNCH POINTS AT, as a `PaneRef`'s two durable halves. (!) THESE ARE
/// SPELLED HERE, IN THE DESKTOP, AND THAT IS THE POINT: which tool `Ctrl+t` opens is an
/// application's choice, so it is written in the weave a maker replaces rather than in the host
/// they cannot. A desktop that pointed it at a different terminal would change one line here.
constexpr const char* kTerminalOffice = "zengine.terminal";
constexpr const char* kTerminalPane = "terminal";

using zengine::workshop::pane_text::fit;

// =============================================================================
// The weave
// =============================================================================

class DesktopWeave
    : public loom::WeaveBase<
          DesktopWeave, pane::DesktopState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PaneActionRequested,
                       AppActionRequested, PaneInventory, PaneLaunchAnswered, ActionsRefused>,
          loom::Emit<PaneOffered, PaneActions, PaneContent, AppActions, PaneLaunchRequested,
                     DeselectRequested, DesktopFace>> {
public:
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        announce(mail);
    }

    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        announce(mail);
    }

    /// WORKSHOP GRANTS THE LAUNCHER ITS ROOM.
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != pane::kLauncherPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        granted_ = true;
        say(mail);
    }

    /// (*) ONE OF THE APPLICATION ROWS THIS WEAVE DECLARED, ASKED FOR BY NAME. Workshop resolved
    /// the keystroke against the effective keymap -- the maker's override where one is
    /// authored, this weave's declared default otherwise -- so what arrives is the id.
    ///
    /// (!) AND THE DESELECT ANSWER ECHOES THE NUMBER IT ARRIVED ON. The reply reaches Workshop
    /// in a later delivery, by which time the maker may have pressed again; the number is what
    /// makes this word about THIS keystroke and no other (`DeselectRequested` says why).
    void on(const AppActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (asked.id == pane::kActionTerminal) {
            (void)mail.as_role(pane::kDesktopRole)
                .send_to_role(kWorkshopRole,
                              PaneLaunchRequested{kTerminalOffice, kTerminalPane});
            return;
        }
        if (asked.id == pane::kActionPanes) {
            // (!) THE LAUNCHER LAUNCHES ITSELF THROUGH THE SAME DOOR, and that is deliberate:
            // there is no privileged path by which this weave puts its own pane on the desk.
            // It asks, the host judges room and authority, and the answer may be a refusal
            // this weave has to show like any other.
            (void)mail.as_role(pane::kDesktopRole)
                .send_to_role(kWorkshopRole,
                              PaneLaunchRequested{pane::kDesktopRole, pane::kLauncherPane});
            return;
        }
        if (asked.id == pane::kActionDeselect) {
            (void)mail.as_role(pane::kDesktopRole)
                .send_to_role(kWorkshopRole, DeselectRequested{}, mail.correlation());
            return;
        }
        // AN ID THIS WEAVE NEVER DECLARED IS NO ACT. It spends nothing and says nothing --
        // the pane protocol's own rule, one scope out.
    }

    /// ONE OF THE LAUNCHER PANE'S OWN ROWS, while it holds the keyboard.
    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || asked.pane != pane::kLauncherPane) {
            return;
        }
        if (asked.id == pane::kActionUp) {
            if (state_.cursor > 0) {
                --state_.cursor;
            }
            notice_.clear();
        } else if (asked.id == pane::kActionDown) {
            if (state_.cursor + 1 < static_cast<std::int64_t>(known_.size())) {
                ++state_.cursor;
            }
            notice_.clear();
        } else if (asked.id == pane::kActionLaunch) {
            launch_cursor(mail);
        } else {
            return;
        }
        say(mail);
    }

    /// WHAT PANES THERE ARE, SAID BY THE HOST. Replaced WHOLE, never merged: the host publishes
    /// the current inventory and a row that stopped being returned stopped being in it. This
    /// weave keeps no copy it edits -- a launcher that did would be a second owner of the
    /// population, which is the defect this whole arc exists to avoid.
    void on(const PaneInventory& said, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        known_ = said.panes;
        heard_ = true;
        if (state_.cursor >= static_cast<std::int64_t>(known_.size())) {
            state_.cursor = known_.empty() ? 0 : static_cast<std::int64_t>(known_.size()) - 1;
        }
        say(mail);
        face(mail);
    }

    /// WHAT A LAUNCH CAME TO. A refusal is shown in this weave's own room and on the floor,
    /// because the maker who pressed the key is the one owed the sentence.
    void on(const PaneLaunchAnswered& answer, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        notice_ = answer.refusal;
        say(mail);
    }

    /// (*) WORKSHOP REFUSED ONE OF THIS WEAVE'S DECLARATIONS (BL-WORK-04). It is kept and shown
    /// rather than acted on: this weave's recovery policy is to TELL THE MAKER, because a
    /// desktop that silently rebound itself would leave them pressing a key that no longer
    /// does what the documentation says. Nothing is re-declared and no gesture is guessed.
    void on(const ActionsRefused& said, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        declaration_refusal_ = said.refusal;
        say(mail);
        face(mail);
    }

private:
    // ---- Offering and declaring ---------------------------------------------------------

    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kLauncherPane, pane::kLauncherName,
                                                     pane::kLauncherSummary});
        PaneActions actions;
        actions.pane = pane::kLauncherPane;
        actions.rows = pane_rows();
        (void)mail.as_role(pane::kDesktopRole).send_to_role(kWorkshopRole, actions);
        // ...AND THE APPLICATION'S OWN ROWS, WHICH ARE NOT THE PANE'S. The pane's rows act
        // only while a maker has pressed into the launcher; these act wherever the maker is
        // standing, which is the whole difference between a tool's keys and an application's
        // defaults (WL-KEY-16).
        AppActions app;
        app.rows = app_rows();
        (void)mail.as_role(pane::kDesktopRole).send_to_role(kWorkshopRole, app);
        face(mail);
    }

    /// THE THREE APPLICATION ROWS, AND WHY EACH GESTURE.
    ///
    /// `Ctrl+t` -- the Terminal's own chord, restored. It was `workshop.terminal` before the
    /// overlay retired, so a maker who authored an override for THAT id finds it names nothing
    /// and must move it here; the id changed because the OWNER changed, and pretending
    /// otherwise would be a host row wearing a weave's name.
    ///
    /// `Ctrl+p` -- the launcher. A plain ctrl+letter, which is what the POSIX wire can say
    /// (ctrl+shift+letter cannot be said at all), and the letter the retired picker used, so a
    /// maker's hand goes to the same key for the same idea.
    ///
    /// `Escape` -- in the DEFAULT class, which is what keeps every pane's own Escape its own.
    static std::vector<AppActionRow> app_rows() {
        return {
            AppActionRow{pane::kActionTerminal, "terminal", input::scan::kT, input::mod::kCtrl,
                         ws::app_precedence::kAboveModes},
            AppActionRow{pane::kActionPanes, "panes", input::scan::kP, input::mod::kCtrl,
                         ws::app_precedence::kAboveModes},
            AppActionRow{pane::kActionDeselect, "put down", input::scan::kEscape,
                         input::mod::kNone, ws::app_precedence::kDefault}};
    }

    /// THE LAUNCHER'S OWN THREE. Bare keys are legal here for the reason they were legal in
    /// the picker's context: nothing in this pane takes text.
    static std::vector<PaneActionRow> pane_rows() {
        return {PaneActionRow{pane::kActionUp, "row up", input::scan::kUp, input::mod::kNone},
                PaneActionRow{pane::kActionDown, "row down", input::scan::kDown,
                              input::mod::kNone},
                PaneActionRow{pane::kActionLaunch, "open or focus", input::scan::kReturn,
                              input::mod::kNone}};
    }

    // ---- Launching from the list ---------------------------------------------------------

    void launch_cursor(loom::Mail& mail) {
        if (state_.cursor < 0 || state_.cursor >= static_cast<std::int64_t>(known_.size())) {
            return;
        }
        const InventoryPane& row = known_[static_cast<std::size_t>(state_.cursor)];
        notice_.clear();
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneLaunchRequested{row.office, row.pane});
    }

    // ---- What the launcher shows ----------------------------------------------------------

    void say(loom::Mail& mail) {
        if (!granted_ || rows_ <= 0 || columns_ <= 0) {
            return;
        }
        std::vector<surface::SurfaceTextRow> out;
        const auto push = [&out, this](std::string text, std::int64_t role) {
            if (static_cast<std::int64_t>(out.size()) >= rows_) {
                return;
            }
            out.push_back(surface::SurfaceTextRow{fit(std::move(text), columns_), role});
        };
        if (!heard_) {
            // THE HOST HAS NOT SAID ANYTHING YET, WHICH IS NOT THE SAME AS THERE BEING NO
            // PANES. An empty list here would read as a Workshop with no tools in it.
            push("PANES (waiting)", surface::role::kMuted);
        } else {
            push("PANES -- " + std::to_string(known_.size()), surface::role::kAccent);
            for (std::size_t i = 0; i < known_.size(); ++i) {
                const InventoryPane& p = known_[i];
                const bool here = static_cast<std::int64_t>(i) == state_.cursor;
                // (!) THREE STATES, NOT TWO, because the host answered three questions. A closed
                // tool can be opened; an unavailable one cannot, and saying "closed" of it
                // would send a maker pressing Return at a pane that is never going to appear.
                std::string mark = p.open ? "[open]" : "[    ]";
                std::int64_t role = p.open ? surface::role::kAccent : surface::role::kFill;
                if (!p.available) {
                    mark = "[gone]";
                    role = surface::role::kAlert;
                } else if (p.waiting) {
                    mark = "[room]";
                    role = surface::role::kMuted;
                }
                push(std::string(here ? "> " : "  ") + mark + " " + p.name, role);
            }
        }
        if (!notice_.empty()) {
            push("  " + notice_, surface::role::kAlert);
        }
        if (!declaration_refusal_.empty()) {
            push("  keys refused: " + declaration_refusal_, surface::role::kAlert);
        }
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneContent{pane::kLauncherPane, std::move(out)});
    }

    // ---- What stands in the empty room ------------------------------------------------------

    /// (*) THE FLOOR. This is the small visible behaviour a replacement changes: rebuild this
    /// weave with different words here, reload it, and the room a maker is looking at says
    /// them -- while every other pane keeps its document, its draft and its unsaved work,
    /// because none of that was ever this weave's.
    void face(loom::Mail& mail) {
        std::vector<surface::SurfaceTextRow> out;
        const auto push = [&out](std::string text, std::int64_t role) {
            out.push_back(surface::SurfaceTextRow{std::move(text), role});
        };
        push("Zen Workshop", surface::role::kAccent);
        push("ctrl+t  terminal      ctrl+p  panes      ctrl+k  hotkeys",
             surface::role::kMuted);
        // (!) AND THE TOOLS THAT ARE NOT HERE ARE NAMED ON THE FLOOR. A maker whose Info pane
        // refused to load meets an empty column and no explanation anywhere; this is the
        // surface that explains it while the rest of the Workshop keeps running. The host
        // supplied the fact (`InventoryPane::available`); this weave decided it belongs here.
        std::vector<std::string> gone;
        for (const InventoryPane& p : known_) {
            if (!p.available) {
                gone.push_back(p.name);
            }
        }
        if (!gone.empty()) {
            std::string line = "unavailable: ";
            for (std::size_t i = 0; i < gone.size(); ++i) {
                line += (i == 0 ? "" : ", ") + gone[i];
            }
            push(line, surface::role::kAlert);
            push("  its provider is not in this Workshop -- build it, then launch again",
                 surface::role::kMuted);
        }
        if (!declaration_refusal_.empty()) {
            push("keys refused: " + declaration_refusal_, surface::role::kAlert);
        }
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, DesktopFace{std::move(out)});
    }

    // ---- State not in the shape ----------------------------------------------------------

    zengine::ActivationCursor activation_;

    /// THE HOST'S LAST INVENTORY, held between publications and owned by nobody here.
    std::vector<InventoryPane> known_;
    bool heard_ = false;

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;
    std::string notice_;
    /// WHAT WORKSHOP SAID ABOUT THIS WEAVE'S OWN DECLARATION, if it refused one. Deliberately
    /// NOT in the state shape: a reloaded image declares again and is judged again, so
    /// carrying the old verdict across would show a refusal that may no longer be true.
    std::string declaration_refusal_;
};

} // namespace

ZEN_EXPORT_WEAVE(DesktopWeave)
