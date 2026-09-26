// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Desktop: a loadable weave that owns what this application does by default -- which
// gestures open or focus which tool, what a key nothing more specific claimed means, what a
// maker reads in the empty room, and what is said about a tool that is not there. It offers the
// Pane Manager (launch, focus, close, and the Pane Creator's acts asked of the host) and the
// Hotkeys pane. None of it is a fact about room, focus, realization or roots, the host's four.
// Workshop law: agents/workshop/desktop.md

// It is not privileged: it cannot open a pane the host's inventory does not hold, cause an
// artifact to load, read another pane's rows, keep a maker from quitting, or take a gesture a
// pane declared it owns -- each is the host's answer. A reload keeps `DesktopState` (the chosen
// row by identity, or the lost choice a successor must not replace); a new image declares
// again and asks for the inventory as it is now.

// Both panes work by mouse and keyboard through the shared list pieces (`component::RowMap`,
// `HeldChoice`, `cursor_window`, `columns`) and the host's menu service
// (`workshop/pane_menu.hpp`). A press names the picture it was aimed at, and a press about an
// older picture is refused in words rather than acted on against whatever moved into its place.

#include "desktop-pane/vocabulary.hpp"
#include "desktop-pane/shortcuts.hpp"

#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/inspection_seam_vocabulary.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "component/columns.hpp"
#include "component/held_choice.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"
#include "component/text_box.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
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
namespace component = zengine::component;
namespace pane_menu = zengine::workshop::pane_menu;

using ws::ActionsJudged;
using ws::ActionsWithdrawn;
using ws::AppActionRequested;
using ws::AppActionRow;
using ws::AppActions;
using ws::DeselectRequested;
using ws::DesktopFace;
using ws::InspectPaneRequested;
using ws::InventoryPane;
using ws::KeymapEditAnswered;
using ws::KeymapEditRequested;
using ws::KeymapRequested;
using ws::KeymapShown;
using ws::MakerPaneAnswered;
using ws::MakerPaneRequested;
using ws::PaneActionRequested;
using ws::PaneActionRow;
using ws::PaneActions;
using ws::PaneButton;
using ws::PaneCatalogRequested;
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
using ws::PaneWheel;
using ws::ShownBinding;

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
using zengine::workshop::pane_text::pad;

/// THE SENTENCE FOR A PRESS THAT NAMED A PICTURE THIS PANE HAS SINCE REPLACED.
constexpr const char* kMovedSentence = "the list moved -- press again";

/// A SENTENCE ANOTHER PARTY WROTE, MADE DRAWABLE: the host refuses a row carrying a byte a canvas
/// cannot draw, and a loader's refusal may quote a file's bytes or break a line. What this
/// pane prints of another's words is one row of printable text.
std::string drawable(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char b = static_cast<unsigned char>(text[i]);
        if (b < 0x20 || b == 0x7f) {
            out += ' ';
        } else if (b >= 0x80) {
            // ONE MARK PER MULTI-BYTE SEQUENCE: the lead byte says how long it is.
            out += '?';
            std::size_t more = b >= 0xf0 ? 3 : b >= 0xe0 ? 2 : b >= 0xc0 ? 1 : 0;
            while (more > 0 && i + 1 < text.size() &&
                   (static_cast<unsigned char>(text[i + 1]) & 0xc0) == 0x80) {
                ++i;
                --more;
            }
        } else {
            out += text[i];
        }
    }
    return out;
}

/// THE SEPARATOR INSIDE A MENU SUBJECT: a `PaneRef`'s two halves, or a binding's three, joined
/// by a byte neither a pane key nor a gesture spelling may contain.
constexpr char kSubjectSep = '\x1f';

std::string join_subject(const std::vector<std::string>& parts) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            out += kSubjectSep;
        }
        out += parts[i];
    }
    return out;
}

std::vector<std::string> split_subject(const std::string& subject) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (true) {
        const std::size_t sep = subject.find(kSubjectSep, at);
        out.push_back(subject.substr(at, sep == std::string::npos ? std::string::npos : sep - at));
        if (sep == std::string::npos) {
            return out;
        }
        at = sep + 1;
    }
}

// ---- The Pane Manager's rows, read backwards ---------------------------------------------------

namespace launcher_row {
constexpr std::int64_t kHeading = 0;
constexpr std::int64_t kNameLine = 1;
constexpr std::int64_t kMark = 2;    ///< the `[open]`/`[    ]` control: show or hide
constexpr std::int64_t kName = 3;    ///< the row: choose, or open/focus on a deliberate second press
constexpr std::int64_t kMarker = 4;  ///< `^ n more above` / `v n more below`
constexpr std::int64_t kNote = 5;
} // namespace launcher_row

struct LauncherMeaning {
    std::int64_t kind = launcher_row::kHeading;
    std::size_t index = 0; ///< the inventory row, for a mark or a name
    /// THE ROW'S STABLE SUBJECT (`office\x1fpane`), for a name or a mark; empty otherwise. It is
    /// part of the meaning so the PICTURE NUMBER moves when the subject behind a slot changes,
    /// even though the slot's kind, index and geometry did not: an inventory update that swaps two
    /// same-length names leaves the row where it was but makes it MEAN a different pane, and a
    /// press stamped with the old picture must not resolve against the pane that moved in (the
    /// review's first finding). An index alone concealed that.
    std::string ref;
    bool operator==(const LauncherMeaning& o) const {
        return kind == o.kind && index == o.index && ref == o.ref;
    }
};

/// THE ROW THE MARKER HOLDS, by its two durable keys.
struct RefKey {
    std::string office;
    std::string pane;
    bool operator==(const RefKey& o) const { return office == o.office && pane == o.pane; }
};

/// WHERE THE MARK CONTROL SITS ON A LIST ROW: after the two-column marker, six wide.
constexpr std::int64_t kMarkFirst = 2;
constexpr std::int64_t kMarkWidth = 6;

// ---- The Hotkeys table's rows, read backwards --------------------------------------------------

namespace keys_row {
constexpr std::int64_t kHeading = 0;
constexpr std::int64_t kGroup = 1;
constexpr std::int64_t kBinding = 2;
constexpr std::int64_t kMarker = 3;
constexpr std::int64_t kFooter = 4;
} // namespace keys_row

struct KeysMeaning {
    std::int64_t kind = keys_row::kHeading;
    std::size_t index = 0; ///< the line, for a binding
    /// THE BINDING'S STABLE IDENTITY (`group\x1fid\x1fkey`), for a binding row; empty otherwise.
    /// In the meaning for the same reason as the Pane Manager's `ref`: a different binding at the
    /// same table line is a changed subject, so the picture moves and an old-picture press is
    /// refused rather than resolved against the binding that took the line.
    std::string ref;
    bool operator==(const KeysMeaning& o) const {
        return kind == o.kind && index == o.index && ref == o.ref;
    }
};

/// ONE LINE OF THE TABLE: a group heading or a binding, and the binding's identity for the
/// cursor -- group, id and the key it lists, joined -- so two keys of one action are two rows.
struct KeysLine {
    bool binding = false;
    std::size_t shown = 0; ///< index into the keymap's rows, for a binding
    std::string key;       ///< the cursor's identity; empty for a heading
};

std::string binding_key(const ShownBinding& b) {
    return join_subject({b.group, b.id, b.gesture});
}

// =============================================================================
// The weave
// =============================================================================

// (!) WHAT IT ACCEPTS IS ITS RELOAD CONTRACT. A reload replaces this weave only with an image
// accepting exactly these shapes (Loom's accepted-contract match), so the doors added for the
// mouse -- the press's third version, the wheel, the second button, the menu and toggle answers,
// the edit answer -- make an image built without them a relaunch from a runtime made from one
// build, never a reload (docs/workshop/develop-workshop.md). Images that both accept them reload.
class DesktopWeave
    : public loom::WeaveBase<
          DesktopWeave, pane::DesktopState,
          loom::Accept<ws::PaneShortcuts, loom::Activated, PaneCatalogRequested, PaneRoom, PaneActionRequested,
                       AppActionRequested, PaneInventory, PaneLaunchAnswered,
                       PaneCloseAnswered, PaneToggleAnswered, MakerPaneAnswered, ActionsJudged,
                       ActionsWithdrawn, KeymapShown, KeymapEditAnswered, PaneKey, PaneTextInput,
                       ws::v3::PanePressed, PaneWheel, PaneButton, PaneMenuAnswered,
                       loom::DispatchRefused, surface::ClipboardCopy, surface::ClipboardText>,
          loom::Emit<ws::PaneShortcutsAnswered, ws::PaneShortcutsRequested, ws::PaneShortcutsWithdrawn,
                     ws::PaneShortcutInvoked, PaneOffered, PaneActions, ws::v3::PaneContent, AppActions,
                     PaneLaunchRequested, PaneCloseRequested, PaneToggleRequested,
                     MakerPaneRequested, DeselectRequested, DesktopFace, PaneInventoryRequested,
                     KeymapRequested, KeymapEditRequested, PaneMenuRequested, PanePassRequested,
                     PaneKeyboardRequested, PaneManageRequested, InspectPaneRequested,
                     surface::ClipboardCopy, surface::ClipboardTextRequested>> {
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

    pane::Shortcuts shortcuts_;
    void on(const ws::PaneShortcuts& request, loom::Mail& mail) {
        const auto attempt = ++attempts_;
        if (shortcuts_.propose(request, mail, app_rows(), attempt)) app_.attempt = attempt;
    }
    /// One of the application rows this weave declared, asked for by name: Workshop resolved the
    /// key against the effective keymap, so what arrives is the id. The deselect answer echoes
    /// the number it arrived on, since it reaches Workshop after the maker may have pressed again
    /// (`DeselectRequested` says why).
    void on(const AppActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (asked.id == pane::kActionTerminal) {
            launch(mail, kTerminalOffice, kTerminalPane);
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
            launch(mail, pane::kDesktopRole, pane::kHotkeysPane);
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
            keys_action(asked.id, mail);
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
                notice_ = cancel_sentence(); // said of the line before closing it ends the draft
                close_naming(mail);
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
        } else if (asked.id == pane::kActionMenu) {
            // THE MENU KEY: the same rows a right press on the marked row offers, continuing
            // THIS keystroke (its number is on the envelope), beside the marked row.
            offer_launcher_menu(mail, mail.correlation());
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
    /// vocabulary while one is open, the Hotkeys pane's capture or spelling line while one is
    /// open, and nothing otherwise: a key that means nothing here is no act, and says nothing.
    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (key.pane == pane::kHotkeysPane) {
            keys_key(key, mail);
            return;
        }
        if (key.pane != pane::kLauncherPane || !naming_.open) {
            return;
        }
        if (line_key(naming_.line, key, mail)) {
            say(mail);
        }
    }

    /// TEXT TYPED WHILE THE PANE HOLDS THE KEYBOARD -- into the name line, or the spelling line.
    void on(const PaneTextInput& typed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || typed.text.empty()) {
            return;
        }
        if (typed.pane == pane::kLauncherPane && naming_.open) {
            naming_.line.type(typed.text);
            say(mail);
        } else if (typed.pane == pane::kHotkeysPane && typing_.active) {
            typing_.line.type(typed.text);
            say_keys(mail);
        }
    }

    /// WHAT THE PROCESS SAYS IT COPIED -- this pane's own copies included, which is why it is a
    /// mirror rather than a second store: a paste on a medium that cannot be read answers with it.
    void on(const surface::ClipboardCopy& said, loom::Mail&) { clip_.text = said.text; }

    /// THE SKIN'S ANSWER TO THIS IMAGE'S OWN PASTE, landing only in the line that asked --
    /// the same line, still open, in the same draft (`TextBox::draft_epoch`).
    void on(const surface::ClipboardText& a, loom::Mail& mail) {
        if (!mail.answers_ask() || !paste_.awaiting || mail.correlation() != paste_.pending) {
            return;
        }
        paste_.awaiting = false;
        zengine::component::TextBox* line = pasting_line();
        if (line == nullptr || line->draft_epoch() != paste_.epoch) {
            return; // the line that asked is gone; the text lands nowhere
        }
        const std::string text = a.readable ? a.text : clip_.text;
        if (text.empty()) {
            return;
        }
        line->type(text);
        if (line == &naming_.line) {
            say(mail);
        } else {
            say_keys(mail);
        }
    }

    // WL-MAKER-14 -- agents/workshop/maker-pane.md
    /// What one of the Pane Creator's acts came to: the host's own sentence, as the notice, about
    /// the act `making_` records and no other ask -- a newer ask numbered from the same counter
    /// is not a verdict on this one. An accepted make closes only the line that asked, and only
    /// if nothing came after: the same draft, holding exactly the name it sent, with no paste on
    /// its way into it; otherwise the line and its text stand. A refusal closes nothing.
    void on(const MakerPaneAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask() || !making_.awaiting || mail.correlation() != making_.pending) {
            return;
        }
        const Making was = std::move(making_);
        making_ = Making{};
        notice_ = answer.said;
        if (was.act == ws::maker_pane_act::kCreate && answer.accepted && naming_.open &&
            naming_.line.draft_epoch() == was.draft) {
            const bool pasting = paste_.awaiting && paste_.epoch == was.draft;
            if (naming_.line.text() == was.name && !pasting) {
                close_naming(mail);
            } else {
                naming_.made = was.name;
            }
        }
        say(mail);
    }

    // WL-MAKER-14 -- agents/workshop/maker-pane.md
    /// Loom's word that an ask of this image's never reached its door's handler: an attestation,
    /// not an answer. Provenance first, then the exact queued attempt, its correlation, what it
    /// asked and where, matched against the one record still waiting (`refused_ask`). A refused
    /// act was never the host's, so nothing was made or saved, and the name line and its text
    /// stand; an act the host received and has not answered stays outstanding.
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (shortcuts_.refused(refused, mail)) return;
        if (!mail.dispatch_refused()) {
            return;
        }
        if (refused_ask(making_.awaiting, making_.attempt, making_.pending, refused, mail,
                        MakerPaneRequested::zen_name, MakerPaneRequested::zen_version,
                        kWorkshopRole)) {
            const Making was = std::move(making_);
            making_ = Making{};
            notice_ = std::string(act_word(was.act)) + " not delivered -- nothing changed (" +
                      refused.reason + ")";
            say(mail);
            return;
        }
        if (refused_ask(paste_.awaiting, paste_.attempt, paste_.pending, refused, mail,
                        surface::ClipboardTextRequested::zen_name,
                        surface::ClipboardTextRequested::zen_version, surface::kSkinRole)) {
            paste_.awaiting = false;
            return;
        }
        // ...AND AN EDIT THAT NEVER ARRIVED: released and said, so the table is not left waiting
        // for a verdict on a request the host never handled.
        if (refused_ask(edit_.awaiting, edit_.attempt, edit_.pending, refused, mail,
                        KeymapEditRequested::zen_name, KeymapEditRequested::zen_version,
                        kWorkshopRole)) {
            edit_ = Edit{};
            keys_notice_ = "edit not delivered -- nothing changed (" + refused.reason + ")";
            say_keys(mail);
        }
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

    /// ...AND WHAT A TOGGLE CAME TO: the host said which of the two it did, and the mark says it
    /// when the inventory is next said; only a refusal needs a sentence here.
    void on(const PaneToggleAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != launches_) {
            return;
        }
        notice_ = answer.refusal;
        say(mail);
    }

    /// Workshop's verdict on one of this image's declarations: Loom says it answers an ask of
    /// this incarnation's, and the correlation says which; a verdict on a superseded attempt
    /// changes nothing shown. The recovery policy is to tell the maker -- no re-declaring,
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
        if (said.pane.empty()) shortcuts_.withdrawn(said.refusal, mail);
        d.word = "keys withdrawn: " + said.refusal;
        say(mail);
        say_keys(mail);
        face(mail);
    }

    // ---- The mouse: a press names a picture, a wheel walks the cursor, a right press offers --

    /// A PRIMARY PRESS IN ONE OF THIS WEAVE'S PANES, naming the picture the medium held when the
    /// press was read. A press about an older picture is refused in words -- never resolved
    /// against whatever row has since moved into its place (WL-DESK-14).
    void on(const ws::v3::PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (press.pane == pane::kLauncherPane) {
            launcher_press(press, mail);
        } else if (press.pane == pane::kHotkeysPane) {
            keys_press(press, mail);
        }
    }

    /// THE WHEEL WALKS THE CURSOR, one row per notch, fractions carried (Powers' convention).
    void on(const PaneWheel& wheel, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        if (wheel.pane == pane::kLauncherPane) {
            const std::int64_t notches = take_notches(wheel_, wheel.dy);
            if (notches != 0) {
                step(-notches);
                say(mail);
            }
        } else if (wheel.pane == pane::kHotkeysPane) {
            const std::int64_t notches = take_notches(keys_wheel_, wheel.dy);
            if (notches != 0) {
                keys_step(-notches);
                say_keys(mail);
            }
        }
    }

    /// THE SECOND BUTTON. A right press on a row of either list OFFERS that row's menu, beside
    /// the press, continuing it; a right press on anything that is not a row of this pane's own
    /// (a heading, a marker, a footer, the name line) is handed back to the host, whose own pane
    /// menu answers. A middle press and every release mean nothing here and are consumed.
    void on(const PaneButton& b, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || !b.pressed || b.button != 3) {
            return;
        }
        if (b.pane == pane::kLauncherPane) {
            if (!map_.current(b.picture)) {
                notice_ = kMovedSentence;
                say(mail);
                return;
            }
            const LauncherMeaning* m = map_.at(b.row, b.column);
            if (m != nullptr && (m->kind == launcher_row::kName || m->kind == launcher_row::kMark) &&
                m->index < known_.size()) {
                hold(static_cast<std::int64_t>(m->index));
                notice_.clear();
                offer_launcher_row(m->index, b.row, b.column, mail, mail.correlation());
                say(mail);
                return;
            }
            (void)pane_menu::pass_back(mail, pane::kDesktopRole, pane::kLauncherPane);
            return;
        }
        if (b.pane == pane::kHotkeysPane) {
            if (!keys_map_.current(b.picture)) {
                keys_notice_ = kMovedSentence;
                say_keys(mail);
                return;
            }
            const KeysMeaning* m = keys_map_.at(b.row, b.column);
            if (m != nullptr && m->kind == keys_row::kBinding && m->index < lines_.size() &&
                keymap_.rows[lines_[m->index].shown].remappable) {
                keys_hold(m->index);
                keys_notice_.clear();
                offer_keys_row(m->index, b.row, b.column, mail, mail.correlation());
                say_keys(mail);
                return;
            }
            (void)pane_menu::pass_back(mail, pane::kDesktopRole, pane::kHotkeysPane);
        }
    }

    /// What a menu came to, if it answers one of this image's own asks (`Asked::take`: from the
    /// presenter's office, under an unanswered ask's number, about the pane and subject asked,
    /// once). A reloaded desktop asked nothing, so its predecessor's menus act on nothing -- they
    /// were about rows the predecessor showed. The row is judged against what this weave holds now.
    void on(const PaneMenuAnswered& a, loom::Mail& mail) {
        if (a.pane == pane::kLauncherPane) {
            if (!launcher_asked_.take(mail, a).empty()) {
                launcher_chose(a, mail);
            }
        } else if (a.pane == pane::kHotkeysPane) {
            if (!keys_asked_.take(mail, a).empty()) {
                keys_chose(a, mail);
            }
        }
    }

    /// WHAT AN EDIT CAME TO, in the host's own sentence: applied and written, applied for this
    /// run only, or refused with nothing changed. The table itself moves when the host republishes
    /// the keymap; the sentence is this pane's notice until the maker's next act.
    void on(const KeymapEditAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask() || !edit_.awaiting || mail.correlation() != edit_.pending) {
            return;
        }
        edit_ = Edit{};
        keys_notice_ = answer.sentence;
        if (answer.accepted) {
            refind_id_ = answer.action; // the row's key changed; hold the action's first row
        }
        say_keys(mail);
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

    /// WHOLE NOTCHES OUT OF AN ACCUMULATED WHEEL, the remainder carried.
    static std::int64_t take_notches(double& carried, double dy) {
        carried += dy;
        const std::int64_t notches = static_cast<std::int64_t>(carried);
        carried -= static_cast<double>(notches);
        return notches;
    }

    // ---- Offering, declaring and asking ---------------------------------------------------

    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kLauncherPane, pane::kLauncherName,
                                                     pane::kLauncherSummary, 8, 60});
        declare_manager(mail);
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kHotkeysPane, pane::kHotkeysName,
                                                     pane::kHotkeysSummary, 10, 68});
        declare_keys(mail);
        // ...AND THE APPLICATION'S OWN ROWS, WHICH ARE NOT THE PANE'S. The pane's rows act
        // only while a maker has pressed into the launcher; these act wherever the maker is
        // standing (WL-KEY-16).
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

    /// The Pane Manager's own rows, as the one declaration its mode calls for: bare keys while
    /// nothing takes text; while the name line is open, the line's two keys and no more. `x`
    /// closes, and is not Return's second meaning: Return only opens or focuses. `m` offers the
    /// row's menu. The Pane Creator's keys keep the ids a maker's override names: `n` new, `s`
    /// save, `ctrl+d` discard, and Return and Escape on the name line.
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
                PaneActionRow{pane::kActionMenu, "row menu", input::scan::kM, input::mod::kNone},
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

    /// THE HOTKEYS PANE'S ROWS, BY MODE. Reading: a row cursor, its two ends, and the menu key.
    /// Capturing a key: ONLY cancel, so every other key arrives as a key -- the one being
    /// captured. Taking a spelling: commit and cancel, and every other key reaches the line.
    std::vector<PaneActionRow> keys_rows() const {
        if (capture_.active) {
            return {PaneActionRow{pane::kActionKeysCancel, "cancel", input::scan::kEscape,
                                  input::mod::kNone}};
        }
        if (typing_.active) {
            return {PaneActionRow{pane::kActionKeysCommit, "use this spelling",
                                  input::scan::kReturn, input::mod::kNone},
                    PaneActionRow{pane::kActionKeysCancel, "cancel", input::scan::kEscape,
                                  input::mod::kNone}};
        }
        return {PaneActionRow{pane::kActionKeysUp, "row up", input::scan::kUp, input::mod::kNone},
                PaneActionRow{pane::kActionKeysDown, "row down", input::scan::kDown,
                              input::mod::kNone},
                PaneActionRow{pane::kActionKeysTop, "top", input::scan::kHome, input::mod::kNone},
                PaneActionRow{pane::kActionKeysBottom, "bottom", input::scan::kEnd,
                              input::mod::kNone},
                PaneActionRow{pane::kActionKeysMenu, "row menu", input::scan::kM,
                              input::mod::kNone}};
    }

    void declare_keys(loom::Mail& mail) {
        PaneActions keys;
        keys.pane = pane::kHotkeysPane;
        keys.rows = keys_rows();
        keys_rows_.attempt = ++attempts_;
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, keys, keys_rows_.attempt);
    }

    // ---- The Pane Creator's name line, and the Hotkeys pane's spelling line -----------------

    void open_naming(loom::Mail& mail) {
        naming_.open = true;
        naming_.line.clear();
        naming_.made.clear();
        notice_.clear();
        declare_manager(mail);
    }

    /// THE DRAFT ENDS: its text, and what it made. A make it asked for is still `making_`'s, and
    /// a late paste lands nowhere, because the draft that asked for it is over.
    void close_naming(loom::Mail& mail) {
        naming_.open = false;
        naming_.line.clear();
        naming_.made.clear();
        declare_manager(mail);
    }

    /// THE LINE A PASTE LANDS IN, if one is open: the name line or the spelling line.
    zengine::component::TextBox* pasting_line() {
        if (naming_.open) {
            return &naming_.line;
        }
        if (typing_.active) {
            return &typing_.line;
        }
        return nullptr;
    }

    /// ONE KEY INTO ONE LINE, with the line's own copy, cut and paste. True when the line took it.
    bool line_key(zengine::component::TextBox& line, const PaneKey& key, loom::Mail& mail) {
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (!line.consume(key.scancode, key.modifiers, clip_)) {
            return false;
        }
        if (clip_.writes != copied_before) {
            mail.publish(surface::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_paste(mail, line);
        }
        return true;
    }

    // WL-MAKER-14 -- agents/workshop/maker-pane.md
    /// Ask the host for one of the Creator's acts, under this image's own number, recording which
    /// act (for a make, the draft that asked and the name it sent): the host holds the definition
    /// and every refusal. One act unanswered at a time -- a second is not sent, aloud, and nothing
    /// is touched. The ticket is kept: an invalid one means nothing was queued, so the record is
    /// released at once and the pane says so.
    void ask_maker(loom::Mail& mail, std::int64_t act, const std::string& name) {
        if (making_.awaiting) {
            notice_ = std::string(act_word(act)) +
                      " not sent -- an earlier ask is still unanswered";
            return;
        }
        notice_.clear();
        Making asking;
        asking.awaiting = true;
        asking.pending = ++asks_;
        asking.act = act;
        asking.draft = naming_.line.draft_epoch();
        asking.name = name;
        asking.attempt = mail.as_role(pane::kDesktopRole)
                             .send_to_role(kWorkshopRole, MakerPaneRequested{act, name},
                                           asking.pending);
        if (!asking.attempt.valid()) {
            making_ = Making{};
            notice_ = std::string(act_word(act)) + " not submitted -- nothing was queued";
            return;
        }
        making_ = std::move(asking);
    }

    static const char* act_word(std::int64_t act) {
        return act == ws::maker_pane_act::kSave      ? "save"
               : act == ws::maker_pane_act::kDiscard ? "discard"
                                                     : "make";
    }

    // WL-MAKER-14 -- agents/workshop/maker-pane.md
    /// WHAT CLOSING THE NAME LINE ENDED, and nothing it did not. A make this line asked for that
    /// the host has not answered may still be made, and one it answered was made; a line that
    /// asked for nothing made nothing. Closing a line takes back no pane the host made.
    std::string cancel_sentence() const {
        if (making_.awaiting && making_.act == ws::maker_pane_act::kCreate &&
            making_.draft == naming_.line.draft_epoch()) {
            return "name line closed -- " + making_.name +
                   " was already asked for, and may still be made";
        }
        if (!naming_.made.empty()) {
            return "name line closed -- " + naming_.made + " was already made";
        }
        return "no pane was made";
    }

    /// A PASTE IS AN ASK OF THE SKIN'S, ON ITS OWN RECORD: nothing queued means nothing is on its
    /// way into the line, and neither that nor Loom's refusal of it touches the Creator's act.
    void begin_paste(loom::Mail& mail, const zengine::component::TextBox& line) {
        paste_.pending = ++asks_;
        paste_.epoch = line.draft_epoch();
        paste_.attempt = mail.as_role(pane::kDesktopRole)
                             .send_to_role(surface::kSkinRole, surface::ClipboardTextRequested{},
                                           paste_.pending);
        paste_.awaiting = paste_.attempt.valid();
    }

    /// DOES THIS NOTICE NAME THE ASK A RECORD IS WAITING ON -- the same queued attempt, under the
    /// same correlation, of the shape it sent, to the office it addressed? Every half, because a
    /// sequence alone is a number and the rest is what this pane asked (Info's `refused_ask`).
    static bool refused_ask(bool awaiting, loom::Ticket sent, std::uint64_t pending,
                            const loom::DispatchRefused& refused, const loom::Mail& mail,
                            const char* shape, std::uint32_t version, const char* role) {
        const loom::Ticket attempt = refused.refused_attempt();
        return awaiting && sent.valid() && attempt.valid() && attempt.seq == sent.seq &&
               mail.correlation() == pending && refused.shape == shape &&
               refused.version == static_cast<std::int64_t>(version) && refused.role == role &&
               refused.target.empty();
    }

    // ---- The Pane Manager's cursor, held by identity ------------------------------------------

    // WL-DESK-10 -- agents/workshop/desktop-presenting.md
    /// Find the row the maker chose, in the list as the host just said it, by identity. A choice
    /// whose row left is still a choice: the keys stay in `DesktopState`, the marker holds nothing
    /// and says so, and Return and `x` wait for a new choice, here and in any image a reload hands
    /// the state to. Only a cursor never given a pane takes the row it stands on.
    void find_cursor() {
        choice_.key = RefKey{state_.cursor_office, state_.cursor_pane};
        choice_.chosen = !state_.cursor_office.empty() || !state_.cursor_pane.empty();
        choice_.at = state_.cursor < 0 ? 0 : static_cast<std::size_t>(state_.cursor);
        choice_.find(known_, key_of_pane);
        if (!choice_.lost && choice_.at < known_.size()) {
            held_name_ = known_[choice_.at].name;
        }
        write_choice();
    }

    static RefKey key_of_pane(const InventoryPane& p) { return RefKey{p.office, p.pane}; }

    /// THE CHOICE, WRITTEN TO THE RELOAD SHAPE: where the marker stands and which pane it holds.
    void write_choice() {
        state_.cursor = static_cast<std::int64_t>(choice_.at);
        state_.cursor_office = choice_.chosen ? choice_.key.office : std::string();
        state_.cursor_pane = choice_.chosen ? choice_.key.pane : std::string();
    }

    void hold(std::int64_t at) {
        if (at < 0 || static_cast<std::size_t>(at) >= known_.size()) {
            return;
        }
        choice_.hold(known_, static_cast<std::size_t>(at), key_of_pane);
        held_name_ = known_[choice_.at].name;
        write_choice();
    }

    void step(std::int64_t by) {
        if (known_.empty()) {
            return;
        }
        (void)choice_.step(known_, by, key_of_pane);
        held_name_ = known_[choice_.at].name;
        write_choice();
        notice_.clear();
    }

    void launch_cursor(loom::Mail& mail) {
        if (known_.empty()) {
            return;
        }
        if (!choice_.actionable()) {
            notice_ = "Return opened nothing -- choose a row first";
            return;
        }
        notice_.clear();
        // THE IDENTITY THE MARKER HOLDS, not the index: what the maker sees is what opens.
        launch(mail, choice_.key.office, choice_.key.pane);
    }

    void close_cursor(loom::Mail& mail) {
        if (known_.empty()) {
            return;
        }
        if (!choice_.actionable()) {
            notice_ = "x closed nothing -- choose a row first";
            return;
        }
        notice_.clear();
        close(mail, choice_.key.office, choice_.key.pane);
    }

    /// ASK THE HOST TO OPEN OR FOCUS A PANE, under a number of this image's own, so the answer
    /// that comes back can be read against the launch it answers.
    void launch(loom::Mail& mail, const std::string& office, const std::string& pane_key) {
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneLaunchRequested{office, pane_key}, ++launches_);
    }

    /// ...AND TO TAKE ONE OFF THE DESK; the host judges whether it is there, because participation
    /// is the desk's fact and not this list's copy of it.
    void close(loom::Mail& mail, const std::string& office, const std::string& pane_key) {
        (void)mail.as_role(pane::kDesktopRole)
            .send_to_role(kWorkshopRole, PaneCloseRequested{office, pane_key}, ++launches_);
    }

    // ---- The Pane Manager by mouse ------------------------------------------------------------

    /// THE STABLE SUBJECT OF A PANE MANAGER ROW, built the one way so the picture's spans and the
    /// menu-key's row lookup agree on what a row MEANS. Empty for an index off the end.
    std::string launcher_ref(std::size_t index) const {
        return index < known_.size() ? join_subject({known_[index].office, known_[index].pane})
                                     : std::string();
    }

    /// THE STABLE SUBJECT OF A HOTKEYS TABLE LINE -- the binding's identity, or empty for a line
    /// that is not a binding (a heading) or off the end.
    std::string keys_line_ref(std::size_t line) const {
        if (line < lines_.size() && lines_[line].binding) {
            return binding_key(keymap_.rows[lines_[line].shown]);
        }
        return std::string();
    }

    // WL-DESK-14 -- agents/workshop/desktop-presenting.md
    /// A PRESS ON THE MARK SHOWS OR HIDES THE ROW'S PANE; a press on the name CHOOSES the row; a
    /// DELIBERATE SECOND PRESS on the marked name, with the keys already here, is Return's meaning
    /// -- open, or focus and lift. The first press (the one that brings the keys, or one on an
    /// unmarked name) only moves the marker, so a press never means two things at once.
    void launcher_press(const ws::v3::PanePressed& press, loom::Mail& mail) {
        if (!map_.current(press.picture)) {
            notice_ = kMovedSentence;
            say(mail);
            return;
        }
        const LauncherMeaning* m = map_.at(press.row, press.column);
        if (m == nullptr || m->index >= known_.size()) {
            return; // a heading, a marker, a note, the name line: pointing, and nothing more
        }
        const InventoryPane& p = known_[m->index];
        if (m->kind == launcher_row::kMark) {
            notice_.clear();
            if (!p.available) {
                notice_ = p.pending ? p.name + " is not here yet -- this run is still loading it"
                                    : p.name + " is not available -- its provider is not in this "
                                               "Workshop";
            } else if (p.open) {
                close(mail, p.office, p.pane);
            } else {
                launch(mail, p.office, p.pane);
            }
            say(mail);
            return;
        }
        if (m->kind != launcher_row::kName) {
            return;
        }
        const bool marked_here = choice_.actionable() && choice_.at == m->index;
        if (press.keys_went_here && marked_here) {
            notice_.clear();
            launch(mail, p.office, p.pane);
        } else {
            hold(static_cast<std::int64_t>(m->index));
            notice_.clear();
        }
        say(mail);
    }

    /// THE ROWS A PANE MANAGER ROW OFFERS: what the row's state allows, then management and
    /// inspection through their owners. The subject is the row's `PaneRef`, so the answer is
    /// judged against the inventory as it is when it arrives.
    void offer_launcher_row(std::size_t index, std::int64_t row, std::int64_t column,
                            loom::Mail& mail, std::uint64_t correlation) {
        const InventoryPane& p = known_[index];
        pane_menu::Offer offer(pane::kLauncherPane, join_subject({p.office, p.pane}));
        offer.at(row, column);
        if (p.open) {
            offer.row(pane::kMenuFocus, "focus " + p.name);
            offer.row(pane::kMenuClose, "close " + p.name);
        } else if (p.available) {
            offer.row(pane::kMenuOpen, "open " + p.name);
        }
        offer.row(pane::kMenuManage, "manage...");
        offer.row(pane::kMenuInspect, "inspect in Info");
        launcher_asked_ = offer.continuing(mail, pane::kDesktopRole, correlation);
    }

    /// THE MENU KEY: the marked row's menu, beside the marked row.
    void offer_launcher_menu(loom::Mail& mail, std::uint64_t correlation) {
        if (!choice_.actionable() || choice_.at >= known_.size()) {
            notice_ = "no row is chosen -- choose one, then open its menu";
            return;
        }
        const std::int64_t row =
            map_.row_of(LauncherMeaning{launcher_row::kName, choice_.at, launcher_ref(choice_.at)});
        notice_.clear();
        offer_launcher_row(choice_.at, row < 0 ? 0 : row, 0, mail, correlation);
    }

    /// WHAT A PANE MANAGER MENU CAME TO. The subject names the pane; it must still be in the
    /// list, or the choice is about a row the maker can no longer see.
    void launcher_chose(const PaneMenuAnswered& a, loom::Mail& mail) {
        const std::vector<std::string> parts = split_subject(a.subject);
        if (parts.size() != 2) {
            return;
        }
        const InventoryPane* p = nullptr;
        for (const InventoryPane& each : known_) {
            if (each.office == parts[0] && each.pane == parts[1]) {
                p = &each;
                break;
            }
        }
        if (p == nullptr) {
            notice_ = parts[1] + " left the list -- nothing done";
            say(mail);
            return;
        }
        notice_.clear();
        if (a.id == pane::kMenuOpen || a.id == pane::kMenuFocus) {
            launch(mail, p->office, p->pane);
        } else if (a.id == pane::kMenuClose) {
            close(mail, p->office, p->pane);
        } else if (a.id == pane::kMenuManage) {
            // THE HOST'S OWN PANE MENU ON THAT PANE -- covered, closed or consuming every right
            // press -- continuing this choice (its number is on the envelope).
            (void)pane_menu::manage(mail, pane::kDesktopRole, pane::kLauncherPane, p->office,
                                    p->pane);
        } else if (a.id == pane::kMenuInspect) {
            // INSPECTION THROUGH ITS OWNER: Info's door, which judges the reference itself.
            (void)mail.as_role(pane::kDesktopRole)
                .send_to_role(pane::kInfoRole, InspectPaneRequested{p->office, p->pane});
        }
        say(mail);
    }

    // ---- What the launcher shows ----------------------------------------------------------

    /// THE HEADING, THEN THE LIST THROUGH A WINDOW THAT KEEPS THE MARKER VISIBLE, THEN THE
    /// NOTICES. The notices' rows are reserved before the list is laid out, so feedback is never
    /// cut off below a full list; each cut in the list is counted on its own row. Every row is
    /// recorded in the row map as it is written, and the composition is numbered by it.
    void say(loom::Mail& mail) {
        if (!granted_ || rows_ <= 0 || columns_ <= 0) {
            return;
        }
        std::vector<surface::SurfaceTextRow> out;
        map_.begin();
        const auto push = [&out, this](std::string text, std::int64_t role, LauncherMeaning m) {
            if (static_cast<std::int64_t>(out.size()) < rows_) {
                map_.row(static_cast<std::int64_t>(out.size()), m);
                out.push_back(surface::SurfaceTextRow{fit(std::move(text), columns_), role});
            }
        };
        if (!heard_) {
            // THE HOST HAS NOT SAID ANYTHING YET, WHICH IS NOT THE SAME AS THERE BEING NO
            // PANES. An empty list here would read as a Workshop with no tools in it.
            push("PANES (waiting)", surface::role::kMuted, LauncherMeaning{});
            publish_launcher(mail, std::move(out));
            return;
        }
        push("PANES -- " + std::to_string(known_.size()), surface::role::kAccent,
             LauncherMeaning{});
        // THE NAME LINE, UNDER THE HEADING, while one is open: the window follows the caret, and
        // the caret itself does not cross the seam (the documented loss Info's draft carries).
        if (naming_.open) {
            const std::string prompt = "new pane: ";
            const std::int64_t room =
                columns_ - static_cast<std::int64_t>(prompt.size()) - 1;
            if (room > 0) {
                naming_.line.keep_caret_visible(room);
                push(prompt + naming_.line.visible(room), surface::role::kAccent,
                     LauncherMeaning{launcher_row::kNameLine, 0, {}});
            } else {
                push(prompt, surface::role::kAccent, LauncherMeaning{launcher_row::kNameLine, 0, {}});
            }
        }
        std::vector<std::string> notes;
        // THE MARKER HOLDING NOTHING IS SAID FOR AS LONG AS IT HOLDS NOTHING -- a state, not an
        // event, so a later sentence about something else cannot take its row. The name is the
        // one this image last saw on the row; an image that never saw it says the durable key.
        const std::string lost =
            choice_.lost ? (held_name_.empty() ? state_.cursor_pane : held_name_) +
                               " left the list -- choose a row before Return opens anything"
                         : std::string();
        const std::string* said[] = {&lost, &notice_, &pane_rows_.word, &app_.word};
        for (const std::string* word : said) {
            if (!word->empty()) {
                notes.push_back(drawable(*word));
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
        const component::ListWindow w = component::cursor_window(
            known_.size(), choice_.at, first_, budget > 0 ? static_cast<std::size_t>(budget) : 0);
        first_ = w.first;
        if (w.before > 0 && w.markers > 0) {
            push("  ^ " + std::to_string(w.before) + " more above", surface::role::kMuted,
                 LauncherMeaning{launcher_row::kMarker, 0, {}});
        }
        for (std::size_t i = w.first; i < w.end(); ++i) {
            const InventoryPane& p = known_[i];
            const bool here = i == choice_.at;
            // (!) FOUR STATES, NOT TWO, because the host answered four questions. A closed
            // tool can be opened; an unavailable one cannot, and saying "closed" of it would
            // send a maker pressing Return at a pane that is never going to appear; one the run
            // is still loading is neither, and `[gone]` of it would be a verdict nobody reached.
            std::string mark = p.open ? "[open]" : "[    ]";
            std::int64_t role = p.open ? surface::role::kAccent : surface::role::kFill;
            if (!p.available && p.pending) {
                mark = "[load]";
                role = surface::role::kMuted;
            } else if (!p.available) {
                mark = "[gone]";
                role = surface::role::kAlert;
            } else if (p.waiting) {
                mark = "[room]";
                role = surface::role::kMuted;
            }
            // A MARKER THAT HOLDS NOTHING (its pane left the list) is `?`, not `>`.
            const char* marker = here ? (choice_.lost ? "? " : "> ") : "  ";
            const std::int64_t at = static_cast<std::int64_t>(out.size());
            const std::string text = fit(std::string(marker) + mark + " " + p.name, columns_);
            const std::string ref = launcher_ref(i);
            push(text, role, LauncherMeaning{launcher_row::kName, i, ref});
            // THE MARK IS A CONTROL INSIDE THE ROW: recorded only where the cut left it whole.
            (void)map_.span(at, kMarkFirst, kMarkWidth,
                            component::solid_columns(text, static_cast<std::size_t>(columns_)),
                            LauncherMeaning{launcher_row::kMark, i, ref});
        }
        if (w.after > 0 && w.markers > 0) {
            push("  v " + std::to_string(w.after) + " more below", surface::role::kMuted,
                 LauncherMeaning{launcher_row::kMarker, 1, {}});
        }
        for (std::int64_t i = 0; i < note_rows; ++i) {
            push("  " + notes[static_cast<std::size_t>(i)], surface::role::kAlert,
                 LauncherMeaning{launcher_row::kNote, static_cast<std::size_t>(i), {}});
        }
        publish_launcher(mail, std::move(out));
    }

    void publish_launcher(loom::Mail& mail, std::vector<surface::SurfaceTextRow> rows) {
        ws::v3::PaneContent said;
        said.pane = pane::kLauncherPane;
        said.rows = std::move(rows);
        said.picture = map_.settle();
        (void)mail.as_role(pane::kDesktopRole).send_to_role(kWorkshopRole, said);
    }

    // ---- The Hotkeys pane ---------------------------------------------------------------------

    /// THE TABLE'S LINES: a heading per place a key is answered, then one line per key in force.
    /// The lines are the population the cursor and the window are over.
    void compose_lines() {
        lines_.clear();
        std::string group;
        for (std::size_t i = 0; i < keymap_.rows.size(); ++i) {
            const ShownBinding& b = keymap_.rows[i];
            if (b.group != group) {
                group = b.group;
                lines_.push_back(KeysLine{false, 0, std::string()});
            }
            lines_.push_back(KeysLine{true, i, binding_key(b)});
        }
    }

    static std::string key_of_line(const KeysLine& l) { return l.key; }

    /// FIND THE CURSOR'S ROW IN THE FRESH TABLE, by identity; after an accepted edit the row's
    /// key changed, so the action's first row is held instead of losing the cursor.
    void keys_find() {
        keys_choice_.find(lines_, key_of_line);
        if (keys_choice_.lost && !refind_id_.empty()) {
            for (std::size_t i = 0; i < lines_.size(); ++i) {
                if (lines_[i].binding && keymap_.rows[lines_[i].shown].id == refind_id_) {
                    keys_choice_.hold(lines_, i, key_of_line);
                    break;
                }
            }
        }
        refind_id_.clear();
        // THE CURSOR RESTS ON A BINDING, NEVER ON A HEADING: a choice never made, or one whose
        // row left, takes the first binding at or after where it stands (a heading has no key
        // to offer and no menu to open); a lost row that has a binding under it keeps its place.
        if (!keys_choice_.chosen || keys_choice_.lost ||
            (keys_choice_.at < lines_.size() && !lines_[keys_choice_.at].binding)) {
            const std::size_t from = keys_choice_.at < lines_.size() ? keys_choice_.at : 0;
            bool held = false;
            for (std::size_t i = from; i < lines_.size(); ++i) {
                if (lines_[i].binding) {
                    keys_choice_.hold(lines_, i, key_of_line);
                    held = true;
                    break;
                }
            }
            for (std::size_t i = 0; !held && i < lines_.size(); ++i) {
                if (lines_[i].binding) {
                    keys_choice_.hold(lines_, i, key_of_line);
                    held = true;
                }
            }
        }
    }

    void keys_hold(std::size_t line) {
        if (line < lines_.size() && lines_[line].binding) {
            keys_choice_.hold(lines_, line, key_of_line);
        }
    }

    /// STEP THE CURSOR OVER BINDINGS ONLY, `by` rows (negative is up), skipping headings.
    void keys_step(std::int64_t by) {
        if (lines_.empty()) {
            return;
        }
        std::size_t at = keys_choice_.at < lines_.size() ? keys_choice_.at : lines_.size() - 1;
        const std::int64_t dir = by < 0 ? -1 : 1;
        for (std::int64_t n = by < 0 ? -by : by; n > 0; --n) {
            std::size_t next = at;
            while (true) {
                if (dir < 0 ? next == 0 : next + 1 >= lines_.size()) {
                    break;
                }
                next = dir < 0 ? next - 1 : next + 1;
                if (lines_[next].binding) {
                    at = next;
                    break;
                }
            }
        }
        keys_hold(at);
        keys_notice_.clear();
    }

    void keys_action(const std::string& id, loom::Mail& mail) {
        // WHILE CAPTURING OR SPELLING, ONLY THE DECLARED KEYS ARE ACTS; a raced id is nothing.
        if (capture_.active) {
            if (id == pane::kActionKeysCancel) {
                capture_ = Capture{};
                keys_notice_ = "capture cancelled";
                declare_keys(mail);
                say_keys(mail);
            }
            return;
        }
        if (typing_.active) {
            if (id == pane::kActionKeysCancel) {
                typing_ = Typing{};
                keys_notice_ = "spelling cancelled";
                declare_keys(mail);
                say_keys(mail);
            } else if (id == pane::kActionKeysCommit) {
                const Typing was = typing_;
                typing_ = Typing{};
                declare_keys(mail);
                ask_edit(mail, was.id, was.op, 0, 0, was.line.text());
                say_keys(mail);
            }
            return;
        }
        if (id == pane::kActionKeysUp) {
            keys_step(-1);
        } else if (id == pane::kActionKeysDown) {
            keys_step(+1);
        } else if (id == pane::kActionKeysTop) {
            keys_step(-static_cast<std::int64_t>(lines_.size()));
        } else if (id == pane::kActionKeysBottom) {
            keys_step(static_cast<std::int64_t>(lines_.size()));
        } else if (id == pane::kActionKeysMenu) {
            if (keys_choice_.actionable() && keys_choice_.at < lines_.size() &&
                lines_[keys_choice_.at].binding) {
                const std::int64_t row =
                    keys_map_.row_of(KeysMeaning{keys_row::kBinding, keys_choice_.at,
                                                 keys_line_ref(keys_choice_.at)});
                keys_notice_.clear();
                offer_keys_row(keys_choice_.at, row < 0 ? 0 : row, 0, mail, mail.correlation());
            } else {
                keys_notice_ = "no row is chosen -- choose one, then open its menu";
            }
        } else {
            return;
        }
        say_keys(mail);
    }

    /// A KEY WHILE THE HOTKEYS PANE HOLDS THE KEYBOARD: the captured key, a key into the spelling
    /// line, or nothing.
    void keys_key(const PaneKey& key, loom::Mail& mail) {
        if (capture_.active) {
            if (key.scancode == input::scan::kUnknown) {
                return; // a key this build cannot name; the host would refuse it in words
            }
            const Capture was = capture_;
            capture_ = Capture{};
            declare_keys(mail);
            ask_edit(mail, was.id, was.op, key.scancode, key.modifiers, std::string());
            say_keys(mail);
            return;
        }
        if (typing_.active && line_key(typing_.line, key, mail)) {
            say_keys(mail);
        }
    }

    /// A PRESS ON A TABLE ROW CHOOSES IT -- visible selection, for the menu key to act on.
    void keys_press(const ws::v3::PanePressed& press, loom::Mail& mail) {
        if (!keys_map_.current(press.picture)) {
            keys_notice_ = kMovedSentence;
            say_keys(mail);
            return;
        }
        const KeysMeaning* m = keys_map_.at(press.row, press.column);
        if (m == nullptr || m->kind != keys_row::kBinding) {
            return;
        }
        keys_hold(m->index);
        keys_notice_.clear();
        say_keys(mail);
    }

    /// THE ROWS A BINDING OFFERS. The subject is the binding's identity (group, id, key), so the
    /// answer is judged against the table as it is when it arrives.
    void offer_keys_row(std::size_t line, std::int64_t row, std::int64_t column, loom::Mail& mail,
                        std::uint64_t correlation) {
        const ShownBinding& b = keymap_.rows[lines_[line].shown];
        pane_menu::Offer offer(pane::kHotkeysPane, binding_key(b));
        offer.at(row, column);
        offer.row(pane::kMenuModifyPress, "Modify (press a key)");
        offer.row(pane::kMenuModifyType, "Modify (type a spelling)");
        offer.row(pane::kMenuAddPress, "Add a key (press)");
        offer.row(pane::kMenuAddType, "Add a key (type)");
        if (!b.gesture.empty()) {
            offer.row(pane::kMenuRemove, "Remove `" + b.gesture + "`");
        }
        offer.row(pane::kMenuDisable, "Disable");
        offer.row(pane::kMenuReset, "Reset to default");
        keys_asked_ = offer.continuing(mail, pane::kDesktopRole, correlation);
    }

    /// WHAT A HOTKEYS MENU CAME TO. The subject names a binding, which must still be in the
    /// table -- the file, a declaration or another edit may have moved it while the menu was open.
    void keys_chose(const PaneMenuAnswered& a, loom::Mail& mail) {
        const std::vector<std::string> parts = split_subject(a.subject);
        if (parts.size() != 3) {
            return;
        }
        const ShownBinding* b = nullptr;
        for (const ShownBinding& each : keymap_.rows) {
            if (each.group == parts[0] && each.id == parts[1] && each.gesture == parts[2]) {
                b = &each;
                break;
            }
        }
        if (b == nullptr) {
            keys_notice_ = "that row moved -- open its menu again";
            say_keys(mail);
            return;
        }
        keys_notice_.clear();
        if (a.id == pane::kMenuModifyPress || a.id == pane::kMenuAddPress) {
            // CAPTURE: only cancel is declared, so the next key arrives as a key -- the one to
            // bind. A chord answered above every mode cannot arrive; type it instead.
            capture_ = Capture{true, b->id,
                               a.id == pane::kMenuModifyPress ? ws::keymap_edit::kSet
                                                              : ws::keymap_edit::kAdd};
            declare_keys(mail);
            // THE EDIT NEEDS THE KEYS. The menu left them where they were, so ask for them,
            // continuing this choice -- guarded, so a newer act defeats it (WL-KEY-17).
            (void)pane_menu::take_keyboard(mail, pane::kDesktopRole, pane::kHotkeysPane);
            keys_notice_ = "press the key for `" + b->id + "` (Escape cancels; a chord like " +
                           "ctrl+p that opens a tool must be typed instead)";
        } else if (a.id == pane::kMenuModifyType || a.id == pane::kMenuAddType) {
            typing_ = Typing{};
            typing_.active = true;
            typing_.id = b->id;
            typing_.op = a.id == pane::kMenuModifyType ? ws::keymap_edit::kSetSpelled
                                                       : ws::keymap_edit::kAddSpelled;
            typing_.line.clear();
            declare_keys(mail);
            (void)pane_menu::take_keyboard(mail, pane::kDesktopRole, pane::kHotkeysPane);
            keys_notice_ = "type the key for `" + b->id + "` as the file spells it, then Return";
        } else if (a.id == pane::kMenuRemove) {
            ask_edit(mail, b->id, ws::keymap_edit::kRemoveSpelled, 0, 0, b->gesture);
        } else if (a.id == pane::kMenuDisable) {
            ask_edit(mail, b->id, ws::keymap_edit::kDisable, 0, 0, std::string());
        } else if (a.id == pane::kMenuReset) {
            ask_edit(mail, b->id, ws::keymap_edit::kReset, 0, 0, std::string());
        }
        say_keys(mail);
    }

    /// ASK THE HOST FOR ONE EDIT, under a number of this image's own; one at a time, and the
    /// ticket kept, `ask_maker`'s discipline.
    void ask_edit(loom::Mail& mail, const std::string& id, std::int64_t op, std::int64_t scancode,
                  std::int64_t modifiers, const std::string& text) {
        if (edit_.awaiting) {
            keys_notice_ = "edit not sent -- an earlier edit is still unanswered";
            return;
        }
        Edit asking;
        asking.awaiting = true;
        asking.pending = ++asks_;
        asking.attempt =
            mail.as_role(pane::kDesktopRole)
                .send_to_role(kWorkshopRole,
                              KeymapEditRequested{id, op, scancode, modifiers, text},
                              asking.pending);
        if (!asking.attempt.valid()) {
            keys_notice_ = "edit not submitted -- nothing was queued";
            return;
        }
        edit_ = std::move(asking);
        keys_notice_ = "asked the host to change `" + id + "`";
    }

    /// THE HEADING, THE TABLE THROUGH A WINDOW THAT KEEPS THE CURSOR VISIBLE, THE SPELLING LINE
    /// WHILE ONE IS OPEN, AND THE FOOTER SAYING WHERE A KEY IS MOVED: the file this run reads,
    /// what reading it came to, and the one line a maker writes. Columns are laid out once for
    /// every row, and each cell is recorded so a press can be answered by the column it is in.
    void say_keys(loom::Mail& mail) {
        if (!keys_granted_ || keys_room_rows_ <= 0 || keys_room_columns_ <= 0) {
            return;
        }
        std::vector<surface::SurfaceTextRow> out;
        keys_map_.begin();
        const auto push = [&out, this](std::string text, std::int64_t role, KeysMeaning m) {
            if (static_cast<std::int64_t>(out.size()) < keys_room_rows_) {
                keys_map_.row(static_cast<std::int64_t>(out.size()), m);
                out.push_back(
                    surface::SurfaceTextRow{fit(std::move(text), keys_room_columns_), role});
            }
        };
        if (!keys_heard_) {
            push("HOTKEYS (waiting)", surface::role::kMuted, KeysMeaning{});
            publish_keys(mail, std::move(out));
            return;
        }
        compose_lines();
        keys_find();
        std::size_t bindings = 0;
        for (const ShownBinding& b : keymap_.rows) {
            bindings += b.remappable ? 1u : 0u;
        }
        push("HOTKEYS -- " + std::to_string(bindings) + " in force; * moved by your file; "
             "right-click or `m` on a row to change it",
             surface::role::kAccent, KeysMeaning{});
        // THE SPELLING LINE, UNDER THE HEADING, while one is open.
        if (typing_.active) {
            const std::string prompt = "key for " + typing_.id + ": ";
            const std::int64_t room =
                keys_room_columns_ - static_cast<std::int64_t>(prompt.size()) - 1;
            if (room > 0) {
                typing_.line.keep_caret_visible(room);
                push(prompt + typing_.line.visible(room), surface::role::kAccent,
                     KeysMeaning{keys_row::kFooter, 0, {}});
            } else {
                push(prompt, surface::role::kAccent, KeysMeaning{keys_row::kFooter, 0, {}});
            }
        }
        // WHERE A KEY IS MOVED, said last and reserved first: the grammar a maker writes, the file
        // this run reads, and what reading it came to. A small room keeps the notice alone.
        std::vector<std::string> footer;
        if (!keys_notice_.empty()) {
            footer.push_back("  " + drawable(keys_notice_));
        }
        if (!keys_rows_.word.empty()) {
            footer.push_back("  " + keys_rows_.word);
        }
        footer.push_back("keymap file: " +
                         (keymap_.file.empty() ? std::string("(none)") : keymap_.file));
        if (!keymap_.word.empty()) {
            footer.push_back("  " + drawable(keymap_.word));
        }
        footer.push_back("or write {\"action\": \"<id>\", \"gesture\": \"ctrl+g\"} in the file; "
                         "\"none\" disables; read at launch");
        std::int64_t budget = keys_room_rows_ - 1 - (typing_.active ? 1 : 0);
        // AS MANY FOOTER ROWS AS THE ROOM ALLOWS while three list rows stay -- the notice first,
        // then the reading's word, the file and the grammar; a tiny room keeps the notice alone.
        std::int64_t footer_rows = budget - 3;
        if (footer_rows > static_cast<std::int64_t>(footer.size())) {
            footer_rows = static_cast<std::int64_t>(footer.size());
        }
        if (footer_rows <= 0) {
            footer_rows = !keys_notice_.empty() && budget >= 2 ? 1 : 0;
        }
        budget -= footer_rows;
        // THE COLUMNS, LAID OUT ONCE: key, label, id, marks -- widths from the population, within
        // the room after the two-column marker.
        const std::size_t marks_want = [this] {
            std::size_t w = 0;
            for (const ShownBinding& b : keymap_.rows) {
                w = std::max(w, marks_of(b).size());
            }
            return w;
        }();
        const std::vector<component::Column> asks = {
            component::Column{component::widest(keymap_.rows, [](const ShownBinding& b) {
                                  return b.gesture.empty() ? std::string("(no key)") : b.gesture;
                              }),
                              6},
            component::Column{component::widest(keymap_.rows,
                                                [](const ShownBinding& b) { return b.label; }),
                              8},
            component::Column{component::widest(keymap_.rows,
                                                [](const ShownBinding& b) { return b.id; }),
                              4},
            component::Column{marks_want, marks_want == 0 ? std::size_t{0} : std::size_t{1}}};
        const std::size_t table_budget =
            keys_room_columns_ > 2 ? static_cast<std::size_t>(keys_room_columns_ - 2) : 0;
        const std::vector<std::size_t> widths = component::layout_columns(asks, table_budget);
        const std::vector<std::size_t> offsets = component::column_offsets(widths);
        const component::ListWindow w = component::cursor_window(
            lines_.size(), keys_choice_.at, keys_first_,
            budget > 0 ? static_cast<std::size_t>(budget) : 0);
        keys_first_ = w.first;
        if (w.before > 0 && w.markers > 0) {
            push("  ^ " + std::to_string(w.before) + " more above", surface::role::kMuted,
                 KeysMeaning{keys_row::kMarker, 0, {}});
        }
        for (std::size_t i = w.first; i < w.end(); ++i) {
            const KeysLine& line = lines_[i];
            if (!line.binding) {
                push(keymap_.rows[i + 1 < lines_.size() ? lines_[i + 1].shown : 0].group,
                     surface::role::kAccent, KeysMeaning{keys_row::kGroup, i, {}});
                continue;
            }
            const ShownBinding& b = keymap_.rows[line.shown];
            const bool here = i == keys_choice_.at;
            const std::string cells = component::table_line(
                {b.gesture.empty() ? std::string("(no key)") : b.gesture, b.label, b.id,
                 marks_of(b)},
                widths, [](const std::string& text, std::int64_t width) { return fit(text, width); },
                [](std::string text, std::size_t width) { return pad(std::move(text), width); });
            const std::int64_t at = static_cast<std::int64_t>(out.size());
            const std::string text = fit(std::string(here ? "> " : "  ") + cells, keys_room_columns_);
            const std::string ref = binding_key(b);
            push(text, here ? surface::role::kAccent : surface::role::kFill,
                 KeysMeaning{keys_row::kBinding, i, ref});
            // THE KEY CELL IS A CONTROL INSIDE THE ROW -- the same meaning, recorded so a press on
            // the key is a press on the binding wherever the columns put it.
            (void)keys_map_.span(
                at, 2 + static_cast<std::int64_t>(offsets[0]), static_cast<std::int64_t>(widths[0]),
                component::solid_columns(text, static_cast<std::size_t>(keys_room_columns_)),
                KeysMeaning{keys_row::kBinding, i, ref});
        }
        if (w.after > 0 && w.markers > 0) {
            push("  v " + std::to_string(w.after) + " more below", surface::role::kMuted,
                 KeysMeaning{keys_row::kMarker, 1, {}});
        }
        for (std::int64_t i = 0; i < footer_rows; ++i) {
            push(footer[static_cast<std::size_t>(i)], surface::role::kMuted,
                 KeysMeaning{keys_row::kFooter, static_cast<std::size_t>(i) + 1, {}});
        }
        publish_keys(mail, std::move(out));
    }

    static std::string marks_of(const ShownBinding& b) {
        std::string marks;
        if (b.authored) {
            marks += "*";
        }
        if (!b.remappable) {
            marks += marks.empty() ? "(not remappable)" : " (not remappable)";
        }
        return marks;
    }

    void publish_keys(loom::Mail& mail, std::vector<surface::SurfaceTextRow> rows) {
        ws::v3::PaneContent said;
        said.pane = pane::kHotkeysPane;
        said.rows = std::move(rows);
        said.picture = keys_map_.settle();
        (void)mail.as_role(pane::kDesktopRole).send_to_role(kWorkshopRole, said);
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
        // no key is claimed at all (WL-DESK-11). An action with several keys prints its first.
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
                    break;
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
            if (!p.available && !p.pending) { // still to come is not missing
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
    /// THE NAME THIS IMAGE LAST SAW ON THE CHOSEN ROW, for the sentence if it leaves the list.
    std::string held_name_;
    /// THE CHOICE, HELD BY IDENTITY -- derived from `DesktopState`'s keys whenever the list is
    /// said and written back whenever it moves, so a reload cannot forget it.
    component::HeldChoice<RefKey> choice_;
    /// THE PICTURE READ BACKWARDS, and its number: what a press is judged against.
    component::RowMap<LauncherMeaning> map_;
    /// THE ONE MENU EACH PANE OF THIS IMAGE ASKED FOR AND HAS NOT HEARD ANSWERED. Image-local, and
    /// never in `DesktopState`: a successor must not accept its predecessor's menu (`on(PaneMenuAnswered)`).
    pane_menu::Asked launcher_asked_;
    pane_menu::Asked keys_asked_;
    double wheel_ = 0.0;
    /// THIS IMAGE'S TWO DECLARATIONS AND THE ATTEMPT COUNTER THEY SHARE. Not in the state shape:
    /// a verdict answers only the incarnation that asked (Loom ANS-03), and a new image declares
    /// again and is judged again, so carrying an old verdict across would show one that may no
    /// longer be true.
    std::uint64_t attempts_ = 0;
    Declared pane_rows_;
    Declared keys_rows_;
    Declared app_;
    /// THE HOTKEYS PANE: its room, the keymap the host last said, its lines, its cursor, its
    /// window, its picture, and what it is doing with the keys right now.
    std::int64_t keys_room_rows_ = 0;
    std::int64_t keys_room_columns_ = 0;
    bool keys_granted_ = false;
    KeymapShown keymap_;
    bool keys_heard_ = false;
    std::vector<KeysLine> lines_;
    component::HeldChoice<std::string> keys_choice_;
    std::size_t keys_first_ = 0;
    component::RowMap<KeysMeaning> keys_map_;
    double keys_wheel_ = 0.0;
    std::string keys_notice_;
    std::string refind_id_;
    struct Capture {
        bool active = false;
        std::string id;
        std::int64_t op = 0;
    };
    Capture capture_;
    struct Typing {
        bool active = false;
        std::string id;
        std::int64_t op = 0;
        zengine::component::TextBox line;
    };
    Typing typing_;
    /// THE EDIT AWAITING ITS ANSWER -- at most one, on `ask_maker`'s terms.
    struct Edit {
        bool awaiting = false;
        std::uint64_t pending = 0;
        loom::Ticket attempt{};
    };
    Edit edit_;
    /// THE NUMBER OF THIS IMAGE'S LATEST LAUNCH, CLOSE OR TOGGLE, which the host's answer echoes:
    /// one counter, so an answer to an older request of any kind is history.
    std::uint64_t launches_ = 0;
    /// ...AND THE NUMBER ITS NEXT ASK OF THE PANE CREATOR, THE SKIN OR THE KEYMAP EDIT DOOR GOES
    /// OUT UNDER. One counter, and never a record: which ask an answer is about is the records'
    /// to say (`making_`, `paste_`, `edit_`), so one numbered after another does not replace it.
    std::uint64_t asks_ = 0;
    /// THE PANE CREATOR'S ACT AWAITING ITS ANSWER -- at most one (`ask_maker`): its number, the
    /// queued attempt Loom's refusal would name, which act, and for a make the name line's draft
    /// (`TextBox::draft_epoch`) and the name it sent. Not in the state shape: an answer, and a
    /// refusal notice, reach only the incarnation that asked (Loom ANS-03, MSG-12).
    struct Making {
        bool awaiting = false;
        std::uint64_t pending = 0;
        loom::Ticket attempt{};
        std::int64_t act = 0;
        std::uint64_t draft = 0;
        std::string name;
    };
    Making making_;
    /// THE PANE CREATOR'S NAME LINE: open or not, the line being typed, and the pane this draft
    /// already made while it went on holding later text. Not in the state shape: a reload resets
    /// it, and says nothing about a name nobody made.
    struct Naming {
        bool open = false;
        zengine::component::TextBox line;
        std::string made;
    };
    Naming naming_;
    /// THE CLIPBOARD MIRROR THE LINES' COPY, CUT AND PASTE WORK AGAINST, and the paste in flight.
    zengine::component::Clipboard clip_;
    struct Paste {
        std::uint64_t pending = 0;
        loom::Ticket attempt{};
        std::uint64_t epoch = 0;
        bool awaiting = false;
    };
    Paste paste_;
};

} // namespace

ZEN_EXPORT_WEAVE(DesktopWeave)
