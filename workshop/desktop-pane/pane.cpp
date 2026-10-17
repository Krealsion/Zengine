// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Desktop: a loadable weave that owns what this application does by default -- which
// gestures open or focus which tool, what a key nothing more specific claimed means, and what is
// said about a tool that is not there. It offers the Pane Manager (launch, focus and close, and
// `n` for the View Builder, where a weaver makes a pane of their own), the Hotkeys pane, and the
// Manual a pane's `?` shows. None of it is a fact about room, focus, realization or roots, the
// host's four.
// Workshop law: agents/workshop/desktop.md

// It is not privileged: it cannot open a pane the host's inventory does not hold, cause an
// artifact to load, read another pane's rows, keep a weaver from quitting, or take a gesture a
// pane declared it owns -- each is the host's answer. A reload keeps `DesktopState` (the chosen
// row by identity, or the lost choice a successor must not replace); a new image declares
// again and asks for the inventory as it is now.

// This file is the weave: it offers the three panes, declares their rows and the application's,
// numbers every declaration and every launch, and hands each message to the pane it is about --
// `launcher_view.hpp`, `hotkeys_view.hpp` and `manual_view.hpp`, over `desktop_common.hpp`. A
// press names the picture it was aimed at, and a press about an older picture is refused in
// words rather than acted on against whatever moved into its place.

#include "workshop/desktop-pane/desktop_common.hpp"
#include "workshop/desktop-pane/hotkeys_view.hpp"
#include "workshop/desktop-pane/launcher_view.hpp"
#include "workshop/desktop-pane/manual_view.hpp"
#include "workshop/desktop-pane/vocabulary.hpp"
#include "workshop/desktop-pane/shortcuts.hpp"

#include "manuals/zengine.desktop.hpp" // this weave's manual, compiled in from its page

#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/inspection_seam_vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_document.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_escape.hpp"
#include "workshop/pane_seat.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace input = zengine::input;
namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace pane = zengine::desktop_pane;
namespace pane_menu = zengine::workshop::pane_menu;

using ws::ActionsJudged;
using ws::ActionsWithdrawn;
using ws::AppActionRequested;
using ws::v2::AppActionRow;
using ws::v2::AppActions;
using ws::DeselectRequested;
using ws::InspectPaneRequested;
using ws::PaneSubjectActed;
using ws::KeymapEditAnswered;
using ws::KeymapEditRequested;
using ws::KeymapRequested;
using ws::KeymapShown;
using ws::PaneActionRequested;
using ws::v3::PaneActionRow;
using ws::v3::PaneActions;
using ws::PaneCloseAnswered;
using ws::PaneCloseRequested;
using ws::PaneInventory;
using ws::PaneInventoryRequested;
using ws::PaneKey;
using ws::PaneLaunchAnswered;
using ws::PaneLaunchRequested;
using ws::PaneManageRequested;
using ws::PaneMenuAnswered;
using ws::PaneMenuRequested;
using ws::v2::PaneOffered;
using ws::PaneKeyboardRequested;
using ws::PanePassRequested;
using ws::PaneRoom;
using ws::PaneTextInput;
using ws::PaneToggleAnswered;
using ws::PaneToggleRequested;

using pane::Declared;
using pane::PaneCanvas;
using pane::PanePress;
using pane::from_workshop;
using pane::kTerminalOffice;
using pane::kTerminalPane;
using pane::kWorkshopRole;

// =============================================================================
// The weave
// =============================================================================

/// THE OFFICE OF THE THREE PANES: its activation, its offers on Workshop's catalog request, and
/// the manual of any of the three, the whole page the weave's.
using Office = ws::MultiPaneFacet<pane::kDesktopRole, pane::kLauncherPane,
                                  zengine::manual::pages::zengine_desktop::kManual>;

// (!) WHAT IT ACCEPTS, ITS OFFICE'S DOORS WITH IT, IS ITS RELOAD CONTRACT. A reload replaces this
// weave only with an image accepting exactly these shapes (Loom's accepted-contract match), so
// the doors added for the mouse -- the canvas room and pointer, the menu and toggle answers, the
// edit answer -- make an image built without them a relaunch from a runtime made from one build,
// never a reload (workshop/docs/develop-workshop.md). Images that both accept them reload.
class DesktopWeave
    : public zengine::With<Office>::Weave<
          DesktopWeave, pane::DesktopState,
          loom::Accept<ws::PaneShortcuts, PaneRoom, PaneActionRequested,
                       AppActionRequested, PaneInventory, PaneLaunchAnswered,
                       PaneCloseAnswered, PaneToggleAnswered, ActionsJudged,
                       ActionsWithdrawn, KeymapShown, KeymapEditAnswered, PaneKey, PaneTextInput,
                       ws::PaneCanvasRoom, ws::PaneCanvasPointer, ws::PaneCanvasRejected,
                       PaneMenuAnswered, loom::DispatchRefused, surface::ClipboardCopy,
                       surface::ClipboardText, ws::PaneDocumentShown, PaneSubjectActed>,
          loom::Emit<ws::PaneShortcutsAnswered, ws::PaneShortcutsRequested, ws::PaneShortcutsWithdrawn,
                     ws::PaneShortcutInvoked, PaneOffered, PaneActions, ws::v4::PaneContent,
                     ws::v5::PaneCanvasContent, ws::PaneCaret, AppActions,
                     PaneLaunchRequested, PaneCloseRequested, PaneToggleRequested,
                     DeselectRequested, PaneInventoryRequested,
                     KeymapRequested, KeymapEditRequested, PaneMenuRequested, PanePassRequested,
                     PaneKeyboardRequested, PaneManageRequested, InspectPaneRequested,
                     ws::PaneEscapeUnspent, ws::PaneDocumentRequested,
                     surface::ClipboardCopy, surface::ClipboardTextRequested>> {
public:
    // ---- What its office calls --------------------------------------------------------------

    /// THE THREE PANES IT OFFERS, any of whose manual is the one page this image was built with.
    static bool known_pane(const std::string& pane_key) {
        return pane_key == pane::kLauncherPane || pane_key == pane::kHotkeysPane ||
               pane_key == pane::kManualPane;
    }

    /// THE THREE PANES OFFERED AND DECLARED, the application's rows with them, and what the host
    /// holds now asked for: at each activation and each catalog request of Workshop's.
    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kLauncherPane, pane::kLauncherName,
                                                     pane::kLauncherSummary, 8, 60});
        declare_manager(mail);
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kHotkeysPane, pane::kHotkeysName,
                                                     pane::kHotkeysSummary, 10, 68});
        declare_keys(mail);
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kManualPane, pane::kManualName,
                                                     pane::kManualSummary, 16, 72});
        declare_manual(mail);
        // ...AND THE APPLICATION'S OWN ROWS, WHICH ARE NOT THE PANE'S. The pane's rows act
        // only while a weaver has pressed into the launcher; these act wherever the weaver is
        // standing (WL-DESK-07).
        if (!shortcuts_.pending()) {
            AppActions app;
            app.rows = app_rows(); shortcuts_.append(app.rows);
            app_.attempt = ++attempts_;
            (void)mail.as_role(pane::kDesktopRole).send_to_role(kWorkshopRole, app, app_.attempt);
        }
        mail.as_role(pane::kDesktopRole).publish(ws::PaneShortcutsRequested{});
        // ...AND THE INVENTORY AS IT IS NOW. The publication is said when it changes; a new
        // image arriving while nothing changes would otherwise wait for an unrelated change.
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneInventoryRequested{});
        // ...AND THE KEYMAP IN FORCE, for the same reason: what the Hotkeys pane prints is what
        // the host resolved, never this image's own defaults.
        (void)mail.as_role(pane::kDesktopRole).send_to_role(kWorkshopRole, KeymapRequested{});
    }

    // ---- Its panes' rooms -------------------------------------------------------------------

    /// WORKSHOP GRANTS ONE OF THIS WEAVE'S THREE PANES ITS PROSE ROOM: kept for a host that grants
    /// no canvas, and the room composed for only while the pane holds no canvas room.
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (room.pane == pane::kLauncherPane) {
            launcher_.grant_prose(room);
        } else if (room.pane == pane::kHotkeysPane) {
            hotkeys_.grant_prose(room);
        } else if (room.pane == pane::kManualPane) {
            manual_.grant_prose(room);
        } else {
            return;
        }
        fit_rooms();
        say_pane(room.pane, mail);
    }

    /// ...AND ITS OWN CANVAS: while a pane holds a room there it draws its rows as its picture,
    /// the lattice's rows and columns the room it composes for.
    void on(const ws::PaneCanvasRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (room.pane == pane::kLauncherPane) {
            launcher_.grant_canvas(room);
        } else if (room.pane == pane::kHotkeysPane) {
            hotkeys_.grant_canvas(room);
        } else if (room.pane == pane::kManualPane) {
            manual_.grant_canvas(room);
        } else {
            return;
        }
        fit_rooms();
        say_pane(room.pane, mail);
    }

    // ---- Its panes in graphics windows (its office's `MultiPaneFacet` calls these) ------------

    /// EACH PANE'S SEAT, where a window that shows the pane's area finds it.
    template <class F>
    void each_seat(F&& visit) {
        visit(launcher_.canvas());
        visit(hotkeys_.canvas());
        visit(manual_.canvas());
    }

    /// A pane Workshop seats nowhere, composed again for the room a window gave it.
    void on_window_room(const std::string& pane_key, loom::Mail& mail) { say_pane(pane_key, mail); }

    /// A PRESS ON A PART IN A WINDOW is a primary press on its place in the picture shown there,
    /// which is the pane's current one. The window gives a press to the area's owner alone, so it
    /// is never the press that brought the keys: a second press on the chosen row opens it.
    void on_window_press(const std::string& pane_key, std::int64_t row, std::int64_t column,
                         loom::Mail& mail) {
        primary_press(PanePress{pane_key, row, column, /*keys_went_here=*/true, /*current=*/true},
                      mail);
    }

    /// A refused picture leaves the last good one showing; the next change draws again.
    void on(const ws::PaneCanvasRejected&, loom::Mail&) {}

    pane::Shortcuts shortcuts_;
    void on(const ws::PaneShortcuts& request, loom::Mail& mail) {
        const auto attempt = ++attempts_;
        if (shortcuts_.propose(request, mail, app_rows(), attempt)) app_.attempt = attempt;
    }
    /// One of the application rows this weave declared, asked for by name: Workshop resolved the
    /// key against the effective keymap, so what arrives is the id. The deselect answer echoes
    /// the number it arrived on, since it reaches Workshop after the weaver may have pressed again
    /// (`DeselectRequested` says why).
    void on(const AppActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (asked.id == pane::kActionTerminal) {
            pane::launch(mail, kTerminalOffice, kTerminalPane, launches_);
            return;
        }
        if (asked.id == pane::kActionPanes) {
            // (!) THE LAUNCHER TOGGLES ITSELF THROUGH THE HOST'S DOOR, and that is deliberate:
            // there is no privileged path by which this weave puts its own pane on the desk or
            // takes it off, and the host judges "shown or hidden" against the desk as it is
            // rather than against this weave's last reading of it (WL-DESK-13).
            (void)mail.as_role(pane::kDesktopRole)
                .send_to_role(kWorkshopRole,
                              PaneToggleRequested{pane::kDesktopRole, pane::kLauncherPane},
                              ++launches_);
            return;
        }
        if (asked.id == pane::kActionHotkeys) {
            pane::launch(mail, pane::kDesktopRole, pane::kHotkeysPane, launches_);
            return;
        }
        if (asked.id == pane::kActionManual) {
            manual_.show_manual(asked.office, asked.pane, hotkeys_.keys_heard(), hotkeys_.keymap(),
                                mail, launches_);
            return;
        }
        if (asked.id == pane::kActionDeselect) {
            (void)mail.as_role(pane::kDesktopRole)
                .send_to_role(kWorkshopRole, DeselectRequested{}, mail.correlation());
            return;
        }
        (void)shortcuts_.invoke(asked.id, mail);
    }

    /// ONE OF THIS WEAVE'S PANES' OWN ROWS, while that pane holds the keyboard.
    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (asked.pane == pane::kHotkeysPane) {
            hotkeys_.keys_action(asked.id, keys_rows_, keys_declarer(), mail);
            return;
        }
        if (asked.pane == pane::kManualPane) {
            manual_.manual_action(asked.id, hotkeys_.keys_heard(), hotkeys_.keymap(), mail,
                                  launches_);
            return;
        }
        if (asked.pane != pane::kLauncherPane) {
            return;
        }
        launcher_.launcher_action(asked.id, state_, pane_rows_, app_, mail, launches_);
    }

    /// A KEY THE PANE DECLARED NO ROW FOR, while it holds the keyboard -- the Hotkeys pane's
    /// capture or spelling line while one is open, and nothing otherwise: a key that means nothing
    /// here is no act, and says nothing.
    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        // Escape's default: a list's marker always stands on a row, which is where it rests and
        // not a selection, so Escape goes back to Workshop.
        if (ws::pane_escape::answer(key, mail, pane::kDesktopRole, [] { return false; })) {
            return;
        }
        if (key.pane == pane::kHotkeysPane) {
            hotkeys_.keys_key(key, keys_rows_, keys_declarer(), mail);
        }
    }

    /// TEXT TYPED WHILE THE PANE HOLDS THE KEYBOARD -- into the Hotkeys pane's spelling line.
    void on(const PaneTextInput& typed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || typed.text.empty()) {
            return;
        }
        if (typed.pane == pane::kHotkeysPane) {
            hotkeys_.typed(typed.text, keys_rows_, mail);
        }
    }

    /// WHAT THE PROCESS SAYS IT COPIED, mirrored for the Hotkeys pane's spelling line.
    void on(const surface::ClipboardCopy& said, loom::Mail&) { hotkeys_.copied(said.text); }

    /// THE SKIN'S ANSWER TO THIS IMAGE'S OWN PASTE, for the line that asked.
    void on(const surface::ClipboardText& a, loom::Mail& mail) {
        hotkeys_.pasted(a, keys_rows_, mail);
    }

    /// Loom's word that an ask of this image's never reached its door's handler: an attestation,
    /// not an answer. Provenance first, then the exact queued attempt, its correlation, what it
    /// asked and where, matched against the one record still waiting (`refused_ask`): the
    /// Hotkeys pane's paste and edit, the Pane Manager's inspect, the Manual's ask, in turn.
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (shortcuts_.refused(refused, mail)) return;
        if (!mail.dispatch_refused()) {
            return;
        }
        if (hotkeys_.ask_refused(refused, keys_rows_, mail)) {
            return;
        }
        if (launcher_.inspect_refused(refused, state_, pane_rows_, app_, mail)) {
            return;
        }
        (void)manual_.ask_refused(refused, hotkeys_.keys_heard(), hotkeys_.keymap(), mail);
    }

    /// WHAT PANES THERE ARE, SAID BY THE HOST -- published when it changes, or answered to this
    /// image's own ask. Replaced WHOLE, never merged: this weave keeps no copy it edits.
    void on(const PaneInventory& said, loom::Mail& mail) {
        if (!from_workshop(mail)) {
            return;
        }
        launcher_.inventory_said(said.panes, state_, pane_rows_, app_, mail);
        // ...AND THE MANUAL'S WORDS GO WHEN ITS PANE LEAVES THE DESK: nothing keeps a pane's manual
        // past the `?` that showed it.
        manual_.inventory_said(said.panes);
    }

    /// THE KEYMAP IN FORCE, AS THE HOST RESOLVED IT -- the one binding truth, published when it
    /// changes or answered to this image's ask. The Hotkeys pane is read from it; this weave
    /// keeps no gesture of its own to print (WL-DESK-11).
    void on(const KeymapShown& said, loom::Mail& mail) {
        if (!from_workshop(mail)) {
            return;
        }
        hotkeys_.keymap_said(said, keys_rows_, mail);
        say_manual(mail); // a key the weaver moved, or a spelling, shows in the manual at once
    }

    /// WHAT INFO MADE OF THIS IMAGE'S LATEST INSPECT ASK, and no older one.
    void on(const PaneSubjectActed& said, loom::Mail& mail) {
        launcher_.inspect_answered(said, state_, pane_rows_, app_, mail);
    }

    /// THE MANUAL A PANE ANSWERED, to this image's latest ask and no older one.
    void on(const ws::PaneDocumentShown& shown, loom::Mail& mail) {
        manual_.document_shown(shown, hotkeys_.keys_heard(), hotkeys_.keymap(), mail);
    }

    /// WHAT A LAUNCH CAME TO -- Loom's answer to this image's latest launch, and no older one:
    /// an answer to a launch the weaver has since replaced says nothing about the newer one.
    void on(const PaneLaunchAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != launches_) {
            return;
        }
        launcher_.launch_answered(answer.refusal, state_, pane_rows_, app_, mail);
    }

    /// ...AND WHAT A CLOSE CAME TO, on the same terms: the latest close this image asked for, and
    /// the host's refusal as the notice. A close that happened needs no sentence here -- the row's
    /// own mark moves to `[    ]` when the inventory is said again.
    void on(const PaneCloseAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != launches_) {
            return;
        }
        launcher_.launch_answered(answer.refusal, state_, pane_rows_, app_, mail);
    }

    /// ...AND WHAT A TOGGLE CAME TO: the host said which of the two it did, and the mark says it
    /// when the inventory is next said; only a refusal needs a sentence here.
    void on(const PaneToggleAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != launches_) {
            return;
        }
        launcher_.launch_answered(answer.refusal, state_, pane_rows_, app_, mail);
    }

    /// Workshop's verdict on one of this image's declarations: Loom says it answers an ask of
    /// this incarnation's, and the correlation says which; a verdict on a superseded attempt
    /// changes nothing shown. The recovery policy is to tell the weaver -- no re-declaring,
    /// dropping rows or guessing another gesture, which would leave a key not doing what the
    /// documentation says.
    void on(const ActionsJudged& said, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return; // a verdict is Loom's answer to a declaration of this incarnation's, or nothing
        }
        if (said.pane.empty()) shortcuts_.judged(mail.correlation(), said.accepted, said.refusal, mail);
        Declared& d = declared_for(said.pane);
        if (mail.correlation() != d.attempt) {
            return;
        }
        if (said.accepted) {
            d.in_force = said.declaration;
            d.word.clear();
        } else {
            d.word = "keys refused: " + said.refusal;
        }
        say(mail);
        say_keys(mail);
    }

    /// ...AND A DECLARATION IN FORCE LEAVING THE KEYMAP. Only the number this image was told
    /// names its own rows; any other is a predecessor's or an older declaration's, and not this
    /// image's to show.
    void on(const ActionsWithdrawn& said, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return; // ordinary speech: only the host's office may say it
        }
        Declared& d = declared_for(said.pane);
        if (said.declaration == 0 || said.declaration != d.in_force) {
            return;
        }
        d.in_force = 0;
        if (said.pane.empty()) shortcuts_.withdrawn(said.refusal, mail);
        d.word = "keys withdrawn: " + said.refusal;
        say(mail);
        say_keys(mail);
    }

    // ---- The mouse: a press names a picture, a wheel walks the cursor, a right press offers --

    /// A HAND ON ONE OF THIS WEAVE'S PICTURES, read back to the row and column of its lattice and
    /// fenced by the picture it names: a press about a picture drawn under another meaning is
    /// refused in words -- never resolved against whatever row has since moved into its place
    /// (WL-DESK-14). A primary press is the row's press, a right press its menu, the wheel walks
    /// the cursor; a middle press, motion and every release mean nothing here.
    void on(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        const PaneCanvas* canvas = event.pane == pane::kLauncherPane ? &launcher_.canvas()
                                   : event.pane == pane::kHotkeysPane ? &hotkeys_.canvas()
                                   : event.pane == pane::kManualPane  ? &manual_.canvas()
                                                                       : nullptr;
        const std::optional<ws::RowCell> cell =
            canvas == nullptr ? std::nullopt : canvas->cell_of(event);
        if (!cell) {
            return;
        }
        const ws::RowCell at = *cell;
        const PanePress press{event.pane, at.row, at.column, event.keys_went_here,
                              canvas->shows(event)};
        if (event.phase == ws::canvas_pointer::kWheel) {
            wheel(event.pane, event.dy, mail);
        } else if (event.phase != ws::canvas_pointer::kPress) {
            return;
        } else if (event.button == 1 && at.shown) {
            primary_press(press, mail);
        } else if (event.button == 3) {
            second_button(press, mail);
        }
    }

    /// What a menu came to, if it answers one of this image's own asks (`Asked::take`: from the
    /// presenter's office, under an unanswered ask's number, about the pane and subject asked,
    /// once). A reloaded desktop asked nothing, so its predecessor's menus act on nothing -- they
    /// were about rows the predecessor showed. The row is judged against what this weave holds now.
    void on(const PaneMenuAnswered& a, loom::Mail& mail) {
        if (a.pane == pane::kLauncherPane) {
            launcher_.menu_answered(a, state_, pane_rows_, app_, mail, launches_);
        } else if (a.pane == pane::kHotkeysPane) {
            hotkeys_.menu_answered(a, keys_rows_, keys_declarer(), mail);
        }
    }

    /// WHAT AN EDIT CAME TO, in the host's own sentence, for the Hotkeys pane's notice.
    void on(const KeymapEditAnswered& answer, loom::Mail& mail) {
        hotkeys_.edit_answered(answer, keys_rows_, mail);
    }

    /// WHAT A PART OF THE PANE MANAGER OR THE HOTKEYS TABLE IS CALLED, as that pane names it.
    std::string launcher_part(const pane::LauncherMeaning& m) const {
        return launcher_.launcher_part(m);
    }
    std::string keys_part(const pane::KeysMeaning& m) const { return hotkeys_.keys_part(m); }

private:
    /// A PRIMARY PRESS on a row of the Pane Manager or the Hotkeys table; the Manual takes none.
    void primary_press(const PanePress& press, loom::Mail& mail) {
        if (press.pane == pane::kLauncherPane) {
            launcher_.launcher_press(press, state_, pane_rows_, app_, mail, launches_);
        } else if (press.pane == pane::kHotkeysPane) {
            hotkeys_.keys_press(press, keys_rows_, mail);
        }
    }

    /// THE WHEEL WALKS THE CURSOR of the pane it turned over, one row per notch (Powers').
    void wheel(const std::string& pane_key, double dy, loom::Mail& mail) {
        if (pane_key == pane::kLauncherPane) {
            launcher_.wheel(dy, state_, pane_rows_, app_, mail);
        } else if (pane_key == pane::kHotkeysPane) {
            hotkeys_.wheel(dy, keys_rows_, mail);
        } else if (pane_key == pane::kManualPane) {
            manual_.wheel(dy, hotkeys_.keys_heard(), hotkeys_.keymap(), mail);
        }
    }

    /// THE SECOND BUTTON. A right press on a row of either list OFFERS that row's menu, beside
    /// the press, continuing it; a right press on anything that is not a row of this pane's own
    /// (a heading, a marker, a footer, the name line) is handed back to the host, whose own pane
    /// menu answers.
    void second_button(const PanePress& b, loom::Mail& mail) {
        if (b.pane == pane::kLauncherPane) {
            launcher_.second_button(b, state_, pane_rows_, app_, mail);
        } else if (b.pane == pane::kHotkeysPane) {
            hotkeys_.second_button(b, keys_rows_, mail);
        } else if (b.pane == pane::kManualPane) {
            // The Manual offers no menu of its own: a right press there is the host's.
            (void)pane_menu::pass_back(mail, pane::kDesktopRole, pane::kManualPane);
        }
    }

    /// The rows and columns each pane composes for: its canvas's lattice while it holds one, its
    /// prose room otherwise.
    void fit_rooms() {
        launcher_.fit_room();
        hotkeys_.fit_room();
        manual_.fit_room();
    }

    /// EACH PANE SAYS WHAT IT SHOWS with what it does not own: the reload shape, the words of
    /// this image's declarations, and the keymap the host last said.
    void say(loom::Mail& mail) { launcher_.say(state_, pane_rows_, app_, mail); }
    void say_keys(loom::Mail& mail) { hotkeys_.say_keys(keys_rows_, mail); }
    void say_manual(loom::Mail& mail) {
        manual_.say_manual(hotkeys_.keys_heard(), hotkeys_.keymap(), mail);
    }
    void say_pane(const std::string& pane_key, loom::Mail& mail) {
        if (pane_key == pane::kLauncherPane) {
            say(mail);
        } else if (pane_key == pane::kHotkeysPane) {
            say_keys(mail);
        } else if (pane_key == pane::kManualPane) {
            say_manual(mail);
        }
    }

    /// WHICH OF THIS IMAGE'S DECLARATIONS A VERDICT IS ABOUT, by the pane it names.
    Declared& declared_for(const std::string& pane_key) {
        if (pane_key.empty()) {
            return app_;
        }
        if (pane_key == pane::kManualPane) {
            return manual_rows_;
        }
        return pane_key == pane::kHotkeysPane ? keys_rows_ : pane_rows_;
    }

    // ---- Declaring and asking ---------------------------------------------------------------

    /// THE APPLICATION ROWS, WHY EACH GESTURE, AND WHAT A PRESS CAUSES (`on(AppActionRequested)`).
    /// `Ctrl+t` -- the Terminal's own chord, restored as an application row.
    /// `Ctrl+p` -- the launcher: a plain ctrl+letter, which is what the POSIX wire can say.
    /// `Escape` -- in the DEFAULT class, which is what keeps every pane's own Escape its own.
    /// `Ctrl+f` -- a pane's manual, above every mode: a pane keeps every key it did not declare,
    /// so only a chord reaches past it, and a terminal delivers `?` as no key.
    static std::vector<AppActionRow> app_rows() {
        return {
            AppActionRow{pane::kActionTerminal, "terminal", input::scan::kT, input::mod::kCtrl,
                         ws::app_precedence::kAboveModes,
                         {ws::cause_of<PaneLaunchRequested>(
                             ws::to_pane(kTerminalOffice, kTerminalPane))}},
            AppActionRow{pane::kActionPanes, "panes", input::scan::kP, input::mod::kCtrl,
                         ws::app_precedence::kAboveModes,
                         {ws::cause_of<PaneToggleRequested>(
                             ws::to_pane(pane::kDesktopRole, pane::kLauncherPane))}},
            AppActionRow{pane::kActionHotkeys, "hotkeys", input::scan::kK, input::mod::kCtrl,
                         ws::app_precedence::kAboveModes,
                         {ws::cause_of<PaneLaunchRequested>(
                             ws::to_pane(pane::kDesktopRole, pane::kHotkeysPane))}},
            AppActionRow{pane::kActionDeselect, "put down", input::scan::kEscape,
                         input::mod::kNone, ws::app_precedence::kDefault,
                         {ws::cause_of<DeselectRequested>()}},
            // THE MANUAL OF THE PANE THE ACT WAS PERFORMED ON, asked of its office, and the
            // Manual shown: the ask's subject is the act's own pane, which the request names.
            AppActionRow{pane::kActionManual, "manual", input::scan::kF, input::mod::kCtrl,
                         ws::app_precedence::kAboveModes,
                         {ws::cause_of<ws::PaneDocumentRequested>(),
                          ws::cause_of<PaneLaunchRequested>(
                              ws::to_pane(pane::kDesktopRole, pane::kManualPane))}}};
    }

    /// DECLARE THE PANE MANAGER'S ROWS, under a new attempt number, so the verdict that comes
    /// back is read against this declaration and no older one. It has one mode, its list.
    void declare_manager(loom::Mail& mail) {
        PaneActions actions;
        actions.pane = pane::kLauncherPane;
        actions.mode = pane::kLauncherMode;
        actions.rows = pane::LauncherView::pane_rows();
        pane_rows_.attempt = ++attempts_;
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, actions, pane_rows_.attempt);
    }

    /// THE HOTKEYS PANE'S MODE AND ITS ROWS IN EACH, as the pane says them.
    const char* keys_mode() const { return hotkeys_.keys_mode(); }
    static std::vector<PaneActionRow> keys_rows(const std::string& mode) {
        return pane::HotkeysView::keys_rows(mode);
    }

    /// DECLARE THE HOTKEYS PANE'S ROWS IN THE MODE IT IS IN, and what the other two modes cause,
    /// under a new attempt number -- at announce, and whenever the pane's mode moves.
    void declare_keys(loom::Mail& mail) {
        PaneActions keys;
        keys.pane = pane::kHotkeysPane;
        keys.mode = keys_mode();
        keys.rows = keys_rows(keys.mode);
        keys.elsewhere = ws::causes_elsewhere(
            {pane::kKeysTableMode, pane::kKeysCaptureMode, pane::kKeysSpellMode}, keys.mode,
            [](const std::string& m) { return keys_rows(m); });
        keys_rows_.attempt = ++attempts_;
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, keys, keys_rows_.attempt);
    }

    /// THE HOTKEYS PANE'S OWN ASK TO BE DECLARED AGAIN, handed to it with each act that can
    /// move its mode.
    pane::DeclareKeys keys_declarer() {
        return [this](loom::Mail& m) { declare_keys(m); };
    }

    void declare_manual(loom::Mail& mail) {
        PaneActions rows;
        rows.pane = pane::kManualPane;
        rows.mode = pane::kManualMode;
        rows.rows = pane::ManualView::manual_rows();
        manual_rows_.attempt = ++attempts_;
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, rows, manual_rows_.attempt);
    }

    // ---- State not in the shape ----------------------------------------------------------

    /// THE THREE PANES, each holding its own room, picture, cursor and asks.
    pane::LauncherView launcher_;
    pane::HotkeysView hotkeys_;
    pane::ManualView manual_;
    /// THIS IMAGE'S DECLARATIONS AND THE ATTEMPT COUNTER THEY SHARE. Not in the state shape:
    /// a verdict answers only the incarnation that asked (Loom ANS-03), and a new image declares
    /// again and is judged again, so carrying an old verdict across would show one that may no
    /// longer be true.
    std::uint64_t attempts_ = 0;
    Declared pane_rows_;
    Declared keys_rows_;
    Declared manual_rows_;
    Declared app_;
    /// THE NUMBER OF THIS IMAGE'S LATEST LAUNCH, CLOSE OR TOGGLE, which the host's answer echoes:
    /// one counter, so an answer to an older request of any kind is history.
    std::uint64_t launches_ = 0;
};

} // namespace

ZEN_EXPORT_WEAVE(DesktopWeave)
