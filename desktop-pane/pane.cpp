// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Desktop -- a loadable weave that owns what this application does by default: which
// gestures open or focus which tool, what a key nothing more specific claimed means, what a
// maker reads in the empty room, and what is said about a tool that is not there.
//
// (*) WHAT IT REPLACES, AND WHY EACH PIECE WAS NEVER THE HOST'S. Four things arrive here:
//
//   the `p` picker overlay      a MODE the host owned, which took the keyboard whole and
//   and the host's Pane Manager TOGGLED participation, and a built-in that also inspected.
//                               The Pane Manager is this pane: a launch of an open tool focuses
//                               it, a close is its own key, and making a pane is the Pane
//                               Creator's three acts asked of the host that holds the one
//                               definition. Inspecting a pane is Info's.
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
#include "component/text_box.hpp"
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
using ws::KeymapRequested;
using ws::KeymapShown;
using ws::MakerPaneAnswered;
using ws::MakerPaneRequested;
using ws::ShownBinding;
using ws::PaneActionRequested;
using ws::PaneActionRow;
using ws::PaneActions;
using ws::PaneCatalogRequested;
using ws::PaneCloseAnswered;
using ws::PaneCloseRequested;
using ws::PaneContent;
using ws::PaneInventory;
using ws::PaneInventoryRequested;
using ws::PaneKey;
using ws::PaneLaunchAnswered;
using ws::PaneLaunchRequested;
using ws::PaneOffered;
using ws::PaneRoom;
using ws::PaneTextInput;

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

// WL-DESK-10 -- agents/workshop/desktop-presenting.md
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
                       AppActionRequested, PaneInventory, PaneLaunchAnswered,
                       PaneCloseAnswered, MakerPaneAnswered, ActionsJudged, ActionsWithdrawn,
                       KeymapShown, PaneKey, PaneTextInput, surface::ClipboardCopy,
                       surface::ClipboardText>,
          loom::Emit<PaneOffered, PaneActions, PaneContent, AppActions, PaneLaunchRequested,
                     PaneCloseRequested, MakerPaneRequested, DeselectRequested, DesktopFace,
                     PaneInventoryRequested, KeymapRequested, surface::ClipboardCopy,
                     surface::ClipboardTextRequested>> {
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

    /// WORKSHOP GRANTS ONE OF THIS WEAVE'S TWO PANES ITS ROOM.
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (room.pane == pane::kLauncherPane) {
            rows_ = room.rows;
            columns_ = room.columns;
            granted_ = true;
            say(mail);
        } else if (room.pane == pane::kHotkeysPane) {
            keys_room_rows_ = room.rows;
            keys_room_columns_ = room.columns;
            keys_granted_ = true;
            say_keys(mail);
        }
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
        if (asked.id == pane::kActionHotkeys) {
            launch(mail, pane::kDesktopRole, pane::kHotkeysPane);
            return;
        }
        if (asked.id == pane::kActionDeselect) {
            (void)mail.as_role(pane::kDesktopRole)
                .send_to_role(kWorkshopRole, DeselectRequested{}, mail.correlation());
            return;
        }
        // AN ID THIS WEAVE NEVER DECLARED IS NO ACT. It spends nothing and says nothing.
    }

    /// ONE OF THIS WEAVE'S PANES' OWN ROWS, while that pane holds the keyboard.
    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (asked.pane == pane::kHotkeysPane) {
            scroll_keys(asked.id);
            say_keys(mail);
            return;
        }
        if (asked.pane != pane::kLauncherPane) {
            return;
        }
        // THE NAME LINE OWNS THE PANE'S ACTIONS WHILE IT IS OPEN, AND AN ID IT DOES NOT ANSWER TO
        // IS NO ACT: the declaration and the keystroke race across two messages, so a row this
        // image declared before the line opened (or after it closed) must mean nothing now.
        if (naming_.open) {
            if (asked.id == ws::kCreatorNameId) {
                ask_maker(mail, ws::maker_pane_act::kCreate, naming_.line.text());
            } else if (asked.id == ws::kCreatorCancelId) {
                close_naming(mail);
                notice_ = "no pane was made";
            } else {
                return;
            }
            say(mail);
            return;
        }
        if (asked.id == pane::kActionUp) {
            step(-1);
        } else if (asked.id == pane::kActionDown) {
            step(+1);
        } else if (asked.id == pane::kActionLaunch) {
            launch_cursor(mail);
        } else if (asked.id == pane::kActionClose) {
            close_cursor(mail);
        } else if (asked.id == ws::kCreatorNewId) {
            open_naming(mail);
        } else if (asked.id == ws::kCreatorSaveId) {
            ask_maker(mail, ws::maker_pane_act::kSave, std::string());
        } else if (asked.id == ws::kCreatorDiscardId) {
            ask_maker(mail, ws::maker_pane_act::kDiscard, std::string());
        } else {
            return;
        }
        say(mail);
    }

    /// A KEY THE PANE DECLARED NO ROW FOR, while it holds the keyboard -- the name line's own
    /// vocabulary while one is open, and nothing otherwise: a key that means nothing here is no
    /// act, and says nothing.
    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || key.pane != pane::kLauncherPane ||
            !naming_.open) {
            return;
        }
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (!naming_.line.consume(key.scancode, key.modifiers, clip_)) {
            return;
        }
        if (clip_.writes != copied_before) {
            mail.publish(surface::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_paste(mail);
        }
        say(mail);
    }

    /// TEXT TYPED WHILE THE PANE HOLDS THE KEYBOARD -- into the name line, when one is open.
    void on(const PaneTextInput& typed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || typed.pane != pane::kLauncherPane ||
            !naming_.open || typed.text.empty()) {
            return;
        }
        naming_.line.type(typed.text);
        say(mail);
    }

    /// WHAT THE PROCESS SAYS IT COPIED -- this pane's own copies included, which is why it is a
    /// mirror rather than a second store: a paste on a medium that cannot be read answers with it.
    void on(const surface::ClipboardCopy& said, loom::Mail&) { clip_.text = said.text; }

    /// THE SKIN'S ANSWER TO THIS IMAGE'S OWN PASTE, landing only in the name line that asked --
    /// the same line, still open, in the same draft (`TextBox::draft_epoch`).
    void on(const surface::ClipboardText& a, loom::Mail& mail) {
        if (!mail.answers_ask() || !paste_.awaiting || mail.correlation() != paste_.pending) {
            return;
        }
        paste_.awaiting = false;
        if (!naming_.open || naming_.line.draft_epoch() != paste_.epoch) {
            return; // the line that asked is gone; the text lands nowhere
        }
        const std::string text = a.readable ? a.text : clip_.text;
        if (text.empty()) {
            return;
        }
        naming_.line.type(text);
        say(mail);
    }

    /// WHAT ONE OF THE PANE CREATOR'S ACTS CAME TO -- the host's own sentence, for this image's
    /// latest ask and no older one. A pane that was made closes the name line; a name the host
    /// refused leaves it open, holding what was typed, with the refusal under it.
    void on(const MakerPaneAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != makes_) {
            return;
        }
        notice_ = answer.said;
        if (answer.act == ws::maker_pane_act::kCreate && answer.accepted && naming_.open) {
            close_naming(mail);
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

    /// THE KEYMAP IN FORCE, AS THE HOST RESOLVED IT -- the one binding truth, published when it
    /// changes or answered to this image's ask. The floor's key hints and the Hotkeys pane are
    /// both read from it; this weave keeps no gesture of its own to print (WL-DESK-11).
    void on(const KeymapShown& said, loom::Mail& mail) {
        if (!from_workshop(mail)) {
            return;
        }
        keymap_ = said;
        keys_heard_ = true;
        say_keys(mail);
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

    /// ...AND WHAT A CLOSE CAME TO, on the same terms: the latest close this image asked for, and
    /// the host's refusal as the notice. A close that happened needs no sentence here -- the row's
    /// own mark moves to `[    ]` when the inventory is said again.
    void on(const PaneCloseAnswered& answer, loom::Mail& mail) {
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
        face(mail);
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

    /// ONE OF THIS IMAGE'S DECLARATIONS: the number the latest attempt went out under, the
    /// number Workshop gave the one in force, and what to tell the maker about it.
    struct Declared {
        std::uint64_t attempt = 0;
        std::int64_t in_force = 0;
        std::string word;
    };

    /// WHICH OF THIS IMAGE'S DECLARATIONS A VERDICT IS ABOUT, by the pane it names.
    Declared& declared_for(const std::string& pane_key) {
        if (pane_key.empty()) {
            return app_;
        }
        return pane_key == pane::kHotkeysPane ? keys_rows_ : pane_rows_;
    }

    // ---- Offering, declaring and asking ---------------------------------------------------

    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kLauncherPane, pane::kLauncherName,
                                                     pane::kLauncherSummary});
        declare_manager(mail);
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kHotkeysPane, pane::kHotkeysName,
                                                     pane::kHotkeysSummary});
        PaneActions keys;
        keys.pane = pane::kHotkeysPane;
        keys.rows = keys_rows();
        keys_rows_.attempt = ++attempts_;
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, keys, keys_rows_.attempt);
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
        // ...AND THE KEYMAP IN FORCE, for the same reason: what the floor and the Hotkeys pane
        // print is what the host resolved, never this image's own defaults.
        (void)mail.as_role(pane::kDesktopRole).send_to_role(kWorkshopRole, KeymapRequested{});
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
            AppActionRow{pane::kActionHotkeys, "hotkeys", input::scan::kK, input::mod::kCtrl,
                         ws::app_precedence::kAboveModes},
            AppActionRow{pane::kActionDeselect, "put down", input::scan::kEscape,
                         input::mod::kNone, ws::app_precedence::kDefault}};
    }

    /// THE PANE MANAGER'S OWN ROWS, as the one declaration its mode calls for. Bare keys are
    /// legal while nothing in the pane takes text; while the name line is open the pane takes
    /// text, so it declares the line's two keys and no more, and every other key reaches the line.
    ///
    /// `x` CLOSES, AND IS NOT RETURN'S SECOND MEANING. The picker toggled on one key, so a maker
    /// reaching for an open tool could take it off the desk; Return here only ever opens or
    /// focuses, and taking a pane off is its own deliberate key.
    ///
    /// THE PANE CREATOR'S KEYS ARE THE ONES IT HAD in the host's Pane Manager, under the same ids,
    /// so a maker's authored override finds them: `n` new, `s` save, `ctrl+d` discard (a plain
    /// ctrl+letter, which is what the POSIX wire can say), and Return and Escape on the name line.
    std::vector<PaneActionRow> pane_rows() const {
        if (naming_.open) {
            return {PaneActionRow{ws::kCreatorNameId, "make the pane", input::scan::kReturn,
                                  input::mod::kNone},
                    PaneActionRow{ws::kCreatorCancelId, "cancel", input::scan::kEscape,
                                  input::mod::kNone}};
        }
        return {PaneActionRow{pane::kActionUp, "row up", input::scan::kUp, input::mod::kNone},
                PaneActionRow{pane::kActionDown, "row down", input::scan::kDown,
                              input::mod::kNone},
                PaneActionRow{pane::kActionLaunch, "open or focus", input::scan::kReturn,
                              input::mod::kNone},
                PaneActionRow{pane::kActionClose, "close", input::scan::kX, input::mod::kNone},
                PaneActionRow{ws::kCreatorNewId, "new pane", input::scan::kN, input::mod::kNone},
                PaneActionRow{ws::kCreatorSaveId, "save pane", input::scan::kS,
                              input::mod::kNone},
                PaneActionRow{ws::kCreatorDiscardId, "discard pane edits", input::scan::kD,
                              input::mod::kCtrl}};
    }

    /// DECLARE THE PANE MANAGER'S ROWS AS THEY ARE FOR ITS MODE NOW, under a new attempt number,
    /// so the verdict that comes back is read against this declaration and no older one.
    void declare_manager(loom::Mail& mail) {
        PaneActions actions;
        actions.pane = pane::kLauncherPane;
        actions.rows = pane_rows();
        pane_rows_.attempt = ++attempts_;
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, actions, pane_rows_.attempt);
    }

    // ---- The Pane Creator's name line ------------------------------------------------------

    void open_naming(loom::Mail& mail) {
        naming_.open = true;
        naming_.line.clear();
        notice_.clear();
        declare_manager(mail);
    }

    void close_naming(loom::Mail& mail) {
        naming_.open = false;
        naming_.line.clear(); // the draft's incarnation ends: a late paste lands nowhere
        declare_manager(mail);
    }

    /// ASK THE HOST FOR ONE OF THE CREATOR'S ACTS, under a number of this image's own. The host
    /// holds the definition and every refusal; this pane holds the name line and the keys.
    void ask_maker(loom::Mail& mail, std::int64_t act, const std::string& name) {
        notice_.clear();
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, MakerPaneRequested{act, name}, ++makes_);
    }

    void begin_paste(loom::Mail& mail) {
        paste_.pending = ++makes_;
        paste_.epoch = naming_.line.draft_epoch();
        paste_.awaiting = true;
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(surface::kSkinRole, surface::ClipboardTextRequested{},
                          paste_.pending);
    }

    /// THE HOTKEYS PANE'S OWN FOUR: a scroll, a row at a time or to either end. Nothing in it takes
    /// text, so bare keys are legal.
    static std::vector<PaneActionRow> keys_rows() {
        return {PaneActionRow{pane::kActionKeysUp, "scroll up", input::scan::kUp,
                              input::mod::kNone},
                PaneActionRow{pane::kActionKeysDown, "scroll down", input::scan::kDown,
                              input::mod::kNone},
                PaneActionRow{pane::kActionKeysTop, "top", input::scan::kHome, input::mod::kNone},
                PaneActionRow{pane::kActionKeysBottom, "bottom", input::scan::kEnd,
                              input::mod::kNone}};
    }

    // ---- The cursor, held by identity -------------------------------------------------------

    // WL-DESK-10 -- agents/workshop/desktop-presenting.md
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

    void close_cursor(loom::Mail& mail) {
        if (known_.empty()) {
            return;
        }
        if (lost_ || (state_.cursor_office.empty() && state_.cursor_pane.empty())) {
            notice_ = "x closed nothing -- choose a row first";
            return;
        }
        notice_.clear();
        // THE IDENTITY THE MARKER HOLDS, as for a launch; the host judges whether it is on the
        // desk, because participation is the desk's fact and not this list's copy of it.
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole,
                          PaneCloseRequested{state_.cursor_office, state_.cursor_pane},
                          ++launches_);
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
        // THE NAME LINE, UNDER THE HEADING, while one is open: the window follows the caret, and
        // the caret itself does not cross the seam (the documented loss Info's draft carries).
        if (naming_.open) {
            const std::string prompt = "new pane: ";
            const std::int64_t room =
                columns_ - static_cast<std::int64_t>(prompt.size()) - 1;
            if (room > 0) {
                naming_.line.keep_caret_visible(room);
                push(prompt + naming_.line.visible(room), surface::role::kAccent);
            } else {
                push(prompt, surface::role::kAccent);
            }
        }
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
        std::int64_t budget = rows_ - 1 - (naming_.open ? 1 : 0);
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

    // ---- The Hotkeys pane ---------------------------------------------------------------------

    /// THE KEYMAP AS LINES: a heading per place a key is answered, then its rows -- the key as a
    /// keymap file spells it, the label, the id a file would name, and `*` where the maker's own
    /// file moved or disabled it.
    std::vector<surface::SurfaceTextRow> keys_lines() const {
        std::vector<surface::SurfaceTextRow> lines;
        std::string group;
        for (const ShownBinding& b : keymap_.rows) {
            if (b.group != group) {
                group = b.group;
                lines.push_back(surface::SurfaceTextRow{group, surface::role::kAccent});
            }
            std::string key = b.gesture.empty() ? "(no key)" : b.gesture;
            if (key.size() < 14) {
                key.append(14 - key.size(), ' ');
            }
            std::string text = "  " + key + " " + b.label;
            if (!b.id.empty()) {
                text += "  " + b.id;
            }
            if (b.authored) {
                text += " *";
            }
            if (!b.remappable) {
                text += "  (not remappable)";
            }
            lines.push_back(surface::SurfaceTextRow{text, surface::role::kFill});
        }
        return lines;
    }

    void scroll_keys(const std::string& id) {
        if (id == pane::kActionKeysUp) {
            keys_top_ = keys_top_ > 0 ? keys_top_ - 1 : 0;
        } else if (id == pane::kActionKeysDown) {
            ++keys_top_;
        } else if (id == pane::kActionKeysTop) {
            keys_top_ = 0;
        } else if (id == pane::kActionKeysBottom) {
            keys_top_ = static_cast<std::size_t>(-1); // clamped by the next composition
        }
    }

    /// THE HEADING, THE LIST THROUGH A WINDOW, AND TWO FOOTER ROWS SAYING WHERE A KEY IS MOVED:
    /// the file this run reads, what reading it came to, and the one line a maker writes.
    void say_keys(loom::Mail& mail) {
        if (!keys_granted_ || keys_room_rows_ <= 0 || keys_room_columns_ <= 0) {
            return;
        }
        std::vector<surface::SurfaceTextRow> out;
        const auto push = [&out, this](std::string text, std::int64_t role) {
            if (static_cast<std::int64_t>(out.size()) < keys_room_rows_) {
                out.push_back(
                    surface::SurfaceTextRow{fit(std::move(text), keys_room_columns_), role});
            }
        };
        if (!keys_heard_) {
            push("HOTKEYS (waiting)", surface::role::kMuted);
            (void)mail.as_role(pane::kDesktopRole)
                .send_to_role(kWorkshopRole, PaneContent{pane::kHotkeysPane, std::move(out)});
            return;
        }
        const std::vector<surface::SurfaceTextRow> lines = keys_lines();
        std::size_t bindings = 0;
        for (const ShownBinding& b : keymap_.rows) {
            bindings += b.remappable ? 1u : 0u;
        }
        push("HOTKEYS -- " + std::to_string(bindings) + " in force; * moved by your file",
             surface::role::kAccent);
        // WHERE A KEY IS MOVED, said last and reserved first: the grammar a maker writes, the file
        // this run reads, and what reading it came to. A small room keeps the grammar alone.
        std::vector<std::string> footer;
        footer.push_back("move one: {\"action\": \"<id>\", \"gesture\": \"ctrl+g\"} in the "
                         "keymap file; \"none\" disables it; read at launch");
        footer.push_back("keymap file: " +
                         (keymap_.file.empty() ? std::string("(none)") : keymap_.file));
        if (!keymap_.word.empty()) {
            footer.push_back("  " + keymap_.word);
        }
        std::int64_t budget = keys_room_rows_ - 1;
        const std::int64_t footer_rows =
            budget >= 3 + static_cast<std::int64_t>(footer.size())
                ? static_cast<std::int64_t>(footer.size())
                : (budget >= 4 ? 1 : 0);
        budget -= footer_rows;
        const std::size_t n = lines.size();
        const std::size_t room = budget > 0 ? static_cast<std::size_t>(budget) : 0;
        // A SCROLL, NOT A CURSOR: the first line shown moves, clamped so the last page is full,
        // and each side that is cut says how much on a row of its own.
        std::size_t top = keys_top_;
        if (n <= room) {
            keys_top_ = 0;
            for (const surface::SurfaceTextRow& line : lines) {
                push(line.text, line.role);
            }
        } else if (room >= 3) {
            const std::size_t last_top = n - (room - 1); // the final page: `^` and the rest
            top = top > last_top ? last_top : top;
            keys_top_ = top;
            std::size_t inner = room - (top > 0 ? 1u : 0u);
            const bool below = top + inner < n;
            inner -= below ? 1u : 0u;
            if (top > 0) {
                push("  ^ " + std::to_string(top) + " more above", surface::role::kMuted);
            }
            for (std::size_t i = top; i < top + inner; ++i) {
                push(lines[i].text, lines[i].role);
            }
            if (below) {
                push("  v " + std::to_string(n - top - inner) + " more below",
                     surface::role::kMuted);
            }
        } else if (room > 0) {
            top = top < n ? top : n - 1;
            keys_top_ = top;
            push(lines[top].text, lines[top].role);
        }
        for (std::int64_t i = 0; i < footer_rows; ++i) {
            push(footer[static_cast<std::size_t>(i)], surface::role::kMuted);
        }
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneContent{pane::kHotkeysPane, std::move(out)});
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
        // (!) THE KEYS ARE THE HOST'S ANSWER, NOT THIS WEAVE'S DEFAULTS: a row the maker moved is
        // printed where they moved it, one they disabled says so, and before the keymap is heard
        // no key is claimed at all (WL-DESK-11).
        if (keys_heard_) {
            std::string hints;
            for (const AppActionRow& mine : app_rows()) {
                if (mine.precedence != ws::app_precedence::kAboveModes) {
                    continue;
                }
                for (const ShownBinding& b : keymap_.rows) {
                    if (b.id != mine.id) {
                        continue;
                    }
                    const std::string hint =
                        b.gesture.empty() ? b.label + ": no key" : b.gesture + "  " + b.label;
                    hints += (hints.empty() ? "" : "      ") + hint;
                }
            }
            if (!hints.empty()) {
                push(hints, surface::role::kMuted);
            }
        }
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
    Declared keys_rows_;
    Declared app_;
    /// THE HOTKEYS PANE: its room, the keymap the host last said, and how far it is scrolled.
    std::int64_t keys_room_rows_ = 0;
    std::int64_t keys_room_columns_ = 0;
    bool keys_granted_ = false;
    KeymapShown keymap_;
    bool keys_heard_ = false;
    std::size_t keys_top_ = 0;
    /// THE NUMBER OF THIS IMAGE'S LATEST LAUNCH OR CLOSE, which the host's answer echoes: one
    /// counter, so an answer to an older request of either kind is history.
    std::uint64_t launches_ = 0;
    /// ...AND OF ITS LATEST ASK OF THE PANE CREATOR OR OF THE SKIN, on a counter of their own.
    std::uint64_t makes_ = 0;
    /// THE PANE CREATOR'S NAME LINE: open or not, and the line being typed. Not in the state
    /// shape: a reload resets it, and says nothing about a name nobody made.
    struct Naming {
        bool open = false;
        zengine::component::TextBox line;
    };
    Naming naming_;
    /// THE CLIPBOARD MIRROR THE LINE'S COPY, CUT AND PASTE WORK AGAINST, and the paste in flight.
    zengine::component::Clipboard clip_;
    struct Paste {
        std::uint64_t pending = 0;
        std::uint64_t epoch = 0;
        bool awaiting = false;
    };
    Paste paste_;
};

} // namespace

ZEN_EXPORT_WEAVE(DesktopWeave)
