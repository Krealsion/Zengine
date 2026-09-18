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
//
// (!) WHAT IT KEEPS, AND WHAT IT ASKS FOR AGAIN. A reload keeps `DesktopState` (the row the maker
// was on, by identity). Everything the host said -- the inventory, the verdicts on its
// declarations -- belongs to the image that heard it, so a new image declares again and asks for
// the inventory as it is now, instead of waiting for it to change.

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

using ws::ActionsJudged;
using ws::ActionsWithdrawn;
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
using ws::PaneInventoryRequested;
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

/// WHICH ROWS OF A LIST A ROOM SHOWS: `[first, first + count)`, and whether a counted marker
/// says what is cut above and below. Every marker spends a row of the budget it is in.
struct ListWindow {
    std::size_t first = 0;
    std::size_t count = 0;
    bool above = false;
    bool below = false;
};

// WL-DESK-10 -- agents/workshop/desktop.md
/// THE WINDOW THAT KEEPS `cursor` VISIBLE in at most `budget` rows, markers included -- the
/// largest one, and among those the one nearest `hint` (last time's first row), so a list
/// scrolls by the least it can rather than jumping. The population is bounded (the host's
/// catalog holds at most 32 panes), so every candidate is simply tried.
ListWindow window_for(std::size_t n, std::size_t cursor, std::size_t hint, std::int64_t budget) {
    ListWindow best;
    if (n == 0 || budget <= 0) {
        return best;
    }
    const std::size_t rows = static_cast<std::size_t>(budget);
    if (n <= rows) {
        best.count = n;
        return best;
    }
    bool found = false;
    std::size_t best_distance = 0;
    for (std::size_t first = 0; first <= cursor && first < n; ++first) {
        for (std::size_t count = rows; count >= 1; --count) {
            if (cursor >= first + count || first + count > n) {
                continue;
            }
            const bool above = first > 0;
            const bool below = first + count < n;
            if (count + (above ? 1u : 0u) + (below ? 1u : 0u) > rows) {
                continue;
            }
            const std::size_t distance = first > hint ? first - hint : hint - first;
            if (!found || count > best.count || (count == best.count && distance < best_distance)) {
                best = ListWindow{first, count, above, below};
                best_distance = distance;
                found = true;
            }
            break; // the largest count for this `first`; a smaller one is never better
        }
    }
    if (!found) {
        // A ROOM TOO SMALL FOR THE CURSOR'S ROW AND A MARKER: the cursor's row alone.
        best = ListWindow{cursor, 1, false, false};
    }
    return best;
}

// =============================================================================
// The weave
// =============================================================================

class DesktopWeave
    : public loom::WeaveBase<
          DesktopWeave, pane::DesktopState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PaneActionRequested,
                       AppActionRequested, PaneInventory, PaneLaunchAnswered, ActionsJudged,
                       ActionsWithdrawn>,
          loom::Emit<PaneOffered, PaneActions, PaneContent, AppActions, PaneLaunchRequested,
                     DeselectRequested, DesktopFace, PaneInventoryRequested>> {
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
            launch(mail, kTerminalOffice, kTerminalPane);
            return;
        }
        if (asked.id == pane::kActionPanes) {
            // (!) THE LAUNCHER LAUNCHES ITSELF THROUGH THE SAME DOOR, and that is deliberate:
            // there is no privileged path by which this weave puts its own pane on the desk.
            launch(mail, pane::kDesktopRole, pane::kLauncherPane);
            return;
        }
        if (asked.id == pane::kActionDeselect) {
            (void)mail.as_role(pane::kDesktopRole)
                .send_to_role(kWorkshopRole, DeselectRequested{}, mail.correlation());
            return;
        }
        // AN ID THIS WEAVE NEVER DECLARED IS NO ACT. It spends nothing and says nothing.
    }

    /// ONE OF THE LAUNCHER PANE'S OWN ROWS, while it holds the keyboard.
    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || asked.pane != pane::kLauncherPane) {
            return;
        }
        if (asked.id == pane::kActionUp) {
            step(-1);
        } else if (asked.id == pane::kActionDown) {
            step(+1);
        } else if (asked.id == pane::kActionLaunch) {
            launch_cursor(mail);
        } else {
            return;
        }
        say(mail);
    }

    /// WHAT PANES THERE ARE, SAID BY THE HOST -- published when it changes, or answered to this
    /// image's own ask. Replaced WHOLE, never merged: this weave keeps no copy it edits.
    void on(const PaneInventory& said, loom::Mail& mail) {
        if (!from_workshop(mail)) {
            return;
        }
        known_ = said.panes;
        heard_ = true;
        find_cursor();
        say(mail);
        face(mail);
    }

    /// WHAT A LAUNCH CAME TO -- Loom's answer to this image's latest launch, and no older one:
    /// an answer to a launch the maker has since replaced says nothing about the newer one.
    void on(const PaneLaunchAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != launches_) {
            return;
        }
        notice_ = answer.refusal;
        say(mail);
    }

    /// (*) WORKSHOP'S VERDICT ON ONE OF THIS IMAGE'S DECLARATIONS (BL-WORK-04). Loom says it
    /// answers an ask of this incarnation's, and the correlation says WHICH declaration: a verdict
    /// on an attempt this image has since superseded is history and changes nothing shown.
    ///
    /// THIS WEAVE'S RECOVERY POLICY IS TO TELL THE MAKER. It does not re-declare, drop rows or
    /// guess another gesture: a desktop that silently rebound itself would leave a maker pressing
    /// a key that no longer does what the documentation says.
    void on(const ActionsJudged& said, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return; // a verdict is Loom's answer to a declaration of this incarnation's, or nothing
        }
        Declared& d = said.pane.empty() ? app_ : pane_rows_;
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
        face(mail);
    }

    /// ...AND A DECLARATION IN FORCE LEAVING THE KEYMAP. Only the number this image was told
    /// names its own rows; any other is a predecessor's or an older declaration's, and not this
    /// image's to show.
    void on(const ActionsWithdrawn& said, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return; // ordinary speech: only the host's office may say it
        }
        Declared& d = said.pane.empty() ? app_ : pane_rows_;
        if (said.declaration == 0 || said.declaration != d.in_force) {
            return;
        }
        d.in_force = 0;
        d.word = "keys withdrawn: " + said.refusal;
        say(mail);
        face(mail);
    }

private:
    /// DID THE HOST SAY THIS? Either as its office, or as Loom's answer to an ask this image sent
    /// to that office -- an answer carries answer provenance, not office authorship, and only the
    /// office's holder at delivery could have given it.
    static bool from_workshop(const loom::Mail& mail) {
        return mail.authored_from_role(kWorkshopRole) || mail.answers_ask();
    }

    /// ONE OF THIS IMAGE'S TWO DECLARATIONS: the number the latest attempt went out under, the
    /// number Workshop gave the one in force, and what to tell the maker about it.
    struct Declared {
        std::uint64_t attempt = 0;
        std::int64_t in_force = 0;
        std::string word;
    };

    // ---- Offering, declaring and asking ---------------------------------------------------

    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kLauncherPane, pane::kLauncherName,
                                                     pane::kLauncherSummary});
        PaneActions actions;
        actions.pane = pane::kLauncherPane;
        actions.rows = pane_rows();
        pane_rows_.attempt = ++attempts_;
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, actions, pane_rows_.attempt);
        // ...AND THE APPLICATION'S OWN ROWS, WHICH ARE NOT THE PANE'S. The pane's rows act
        // only while a maker has pressed into the launcher; these act wherever the maker is
        // standing (WL-KEY-16).
        AppActions app;
        app.rows = app_rows();
        app_.attempt = ++attempts_;
        (void)mail.as_role(pane::kDesktopRole).send_to_role(kWorkshopRole, app, app_.attempt);
        // ...AND THE INVENTORY AS IT IS NOW. The publication is said when it changes; a new
        // image arriving while nothing changes would otherwise wait for an unrelated change.
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneInventoryRequested{});
        face(mail);
    }

    /// THE THREE APPLICATION ROWS, AND WHY EACH GESTURE.
    ///
    /// `Ctrl+t` -- the Terminal's own chord, restored as an application row.
    /// `Ctrl+p` -- the launcher: a plain ctrl+letter, which is what the POSIX wire can say.
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

    /// THE LAUNCHER'S OWN THREE. Bare keys are legal here: nothing in this pane takes text.
    static std::vector<PaneActionRow> pane_rows() {
        return {PaneActionRow{pane::kActionUp, "row up", input::scan::kUp, input::mod::kNone},
                PaneActionRow{pane::kActionDown, "row down", input::scan::kDown,
                              input::mod::kNone},
                PaneActionRow{pane::kActionLaunch, "open or focus", input::scan::kReturn,
                              input::mod::kNone}};
    }

    // ---- The cursor, held by identity -------------------------------------------------------

    // WL-DESK-10 -- agents/workshop/desktop.md
    /// FIND THE ROW THE MAKER WAS ON, in the list as the host just said it. By identity, so a
    /// row inserted above it moves the marker with it; a pane that left the list leaves the
    /// marker where it was, holding nothing, and says so -- Return then waits for a choice
    /// rather than acting on whichever pane slid into that place.
    void find_cursor() {
        const std::int64_t n = static_cast<std::int64_t>(known_.size());
        if (!state_.cursor_office.empty() || !state_.cursor_pane.empty()) {
            for (std::int64_t i = 0; i < n; ++i) {
                const InventoryPane& p = known_[static_cast<std::size_t>(i)];
                if (p.office == state_.cursor_office && p.pane == state_.cursor_pane) {
                    state_.cursor = i;
                    held_name_ = p.name;
                    return;
                }
            }
            lost_name_ = held_name_.empty() ? state_.cursor_pane : held_name_;
            state_.cursor_office.clear();
            state_.cursor_pane.clear();
            held_name_.clear();
            lost_ = true;
        }
        if (state_.cursor >= n) {
            state_.cursor = n > 0 ? n - 1 : 0;
        }
        if (state_.cursor < 0) {
            state_.cursor = 0;
        }
        if (!lost_ && n > 0) {
            hold(state_.cursor);
        }
    }

    void hold(std::int64_t at) {
        const InventoryPane& p = known_[static_cast<std::size_t>(at)];
        state_.cursor = at;
        state_.cursor_office = p.office;
        state_.cursor_pane = p.pane;
        held_name_ = p.name;
        lost_ = false;
    }

    void step(std::int64_t by) {
        const std::int64_t n = static_cast<std::int64_t>(known_.size());
        if (n == 0) {
            return;
        }
        std::int64_t at = state_.cursor + by;
        at = at < 0 ? 0 : (at >= n ? n - 1 : at);
        hold(at);
        notice_.clear();
    }

    void launch_cursor(loom::Mail& mail) {
        if (known_.empty()) {
            return;
        }
        if (lost_ || (state_.cursor_office.empty() && state_.cursor_pane.empty())) {
            notice_ = "Return opened nothing -- choose a row first";
            return;
        }
        notice_.clear();
        // THE IDENTITY THE MARKER HOLDS, not the index: what the maker sees is what opens.
        launch(mail, state_.cursor_office, state_.cursor_pane);
    }

    /// ASK THE HOST TO OPEN OR FOCUS A PANE, under a number of this image's own, so the answer
    /// that comes back can be read against the launch it answers.
    void launch(loom::Mail& mail, const std::string& office, const std::string& pane_key) {
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneLaunchRequested{office, pane_key}, ++launches_);
    }

    // ---- What the launcher shows ----------------------------------------------------------

    /// THE HEADING, THEN THE LIST THROUGH A WINDOW THAT KEEPS THE MARKER VISIBLE, THEN THE
    /// NOTICES. The notices' rows are reserved before the list is laid out, so feedback is never
    /// cut off below a full list; each cut in the list is counted on its own row.
    void say(loom::Mail& mail) {
        if (!granted_ || rows_ <= 0 || columns_ <= 0) {
            return;
        }
        std::vector<surface::SurfaceTextRow> out;
        const auto push = [&out, this](std::string text, std::int64_t role) {
            if (static_cast<std::int64_t>(out.size()) < rows_) {
                out.push_back(surface::SurfaceTextRow{fit(std::move(text), columns_), role});
            }
        };
        if (!heard_) {
            // THE HOST HAS NOT SAID ANYTHING YET, WHICH IS NOT THE SAME AS THERE BEING NO
            // PANES. An empty list here would read as a Workshop with no tools in it.
            push("PANES (waiting)", surface::role::kMuted);
            (void)mail.as_role(pane::kDesktopRole)
                .send_to_role(kWorkshopRole, PaneContent{pane::kLauncherPane, std::move(out)});
            return;
        }
        push("PANES -- " + std::to_string(known_.size()), surface::role::kAccent);
        std::vector<std::string> notes;
        // THE MARKER HOLDING NOTHING IS SAID FOR AS LONG AS IT HOLDS NOTHING -- a state, not an
        // event, so a later sentence about something else cannot take its row.
        const std::string lost =
            lost_ ? lost_name_ + " left the list -- choose a row before Return opens anything"
                  : std::string();
        const std::string* said[] = {&lost, &notice_, &pane_rows_.word, &app_.word};
        for (const std::string* word : said) {
            if (!word->empty()) {
                notes.push_back(*word);
            }
        }
        std::int64_t budget = rows_ - 1;
        // AT LEAST ONE LIST ROW STAYS, so the marker is never the thing a notice pushed out.
        const std::int64_t room_for_notes = known_.empty() ? budget : budget - 1;
        const std::int64_t note_rows =
            static_cast<std::int64_t>(notes.size()) < room_for_notes
                ? static_cast<std::int64_t>(notes.size())
                : (room_for_notes > 0 ? room_for_notes : 0);
        budget -= note_rows;
        const ListWindow w = window_for(known_.size(), static_cast<std::size_t>(state_.cursor),
                                        first_, budget);
        first_ = w.first;
        if (w.above) {
            push("  ^ " + std::to_string(w.first) + " more above", surface::role::kMuted);
        }
        for (std::size_t i = w.first; i < w.first + w.count; ++i) {
            const InventoryPane& p = known_[i];
            const bool here = static_cast<std::int64_t>(i) == state_.cursor;
            // (!) THREE STATES, NOT TWO, because the host answered three questions. A closed
            // tool can be opened; an unavailable one cannot, and saying "closed" of it would
            // send a maker pressing Return at a pane that is never going to appear.
            std::string mark = p.open ? "[open]" : "[    ]";
            std::int64_t role = p.open ? surface::role::kAccent : surface::role::kFill;
            if (!p.available) {
                mark = "[gone]";
                role = surface::role::kAlert;
            } else if (p.waiting) {
                mark = "[room]";
                role = surface::role::kMuted;
            }
            // A MARKER THAT HOLDS NOTHING (its pane left the list) is `?`, not `>`.
            const char* marker = here ? (lost_ ? "? " : "> ") : "  ";
            push(std::string(marker) + mark + " " + p.name, role);
        }
        if (w.below) {
            push("  v " + std::to_string(known_.size() - w.first - w.count) + " more below",
                 surface::role::kMuted);
        }
        for (std::int64_t i = 0; i < note_rows; ++i) {
            push("  " + notes[static_cast<std::size_t>(i)], surface::role::kAlert);
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
        // (!) AND THE TOOLS THAT ARE NOT HERE ARE NAMED ON THE FLOOR. The host supplied the
        // fact (`InventoryPane::available`, asked of the office's holder now); this weave
        // decided it belongs here.
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
        for (const Declared* d : {&app_, &pane_rows_}) {
            if (!d->word.empty()) {
                push(d->word, surface::role::kAlert);
            }
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
    /// WHERE THE LIST'S WINDOW BEGAN LAST TIME, so it scrolls by the least it can.
    std::size_t first_ = 0;
    std::string notice_;
    /// THE NAME OF THE PANE THE MARKER HOLDS, for the sentence if it leaves the list.
    std::string held_name_;
    /// THE MARKER'S PANE LEFT THE LIST, and the maker has not chosen another since; and its name.
    bool lost_ = false;
    std::string lost_name_;
    /// THIS IMAGE'S TWO DECLARATIONS AND THE ATTEMPT COUNTER THEY SHARE. Not in the state shape:
    /// a verdict answers only the incarnation that asked (Loom ANS-03), and a new image declares
    /// again and is judged again, so carrying an old verdict across would show one that may no
    /// longer be true.
    std::uint64_t attempts_ = 0;
    Declared pane_rows_;
    Declared app_;
    /// THE NUMBER OF THIS IMAGE'S LATEST LAUNCH, which the host's answer echoes.
    std::uint64_t launches_ = 0;
};

} // namespace

ZEN_EXPORT_WEAVE(DesktopWeave)
