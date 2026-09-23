// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Info pane -- a loadable weave that offers Workshop one pane: the PANES a maker has, and
// the PROPERTIES of the one pane they chose to inspect.
//
// IT WAS THE OBJECT INSPECTOR FIRST. The pane showed the prototype object document -- a picture
// the host derived (`DocumentShown`) and four asks back -- and that document retired with its
// canvas. What it inspects now is a PANE, and the seam has the same three parts: a picture the
// host derives and publishes when it changes (`PaneSubjectShown`), the one door that names the
// subject (`InspectPaneRequested`), and a commit that returns the name of the rows its draft was
// typed for (`PaneCommitRequested`). Nothing in this image can touch a desk or a definition; it
// can ask, and be refused in the owner's words -- or learn from Loom that the ask never arrived.
// (`workshop/inspection_seam_vocabulary.hpp` says who owns what.)
//
// (!) THE SUBJECT IS NAMED HERE, AND NOWHERE ELSE MOVES IT. A pane becomes the subject when the
// maker presses its row, or puts the list cursor on it and presses Return -- in this pane. The
// selection, the pane that holds the keys, a press elsewhere and Escape do not touch it, and
// this pane may name itself: the column that shows a pane's placement can show its own.
//
// THE LIST IS THE ONE INVENTORY, as the host says it (`PaneInventory`, the same reading the
// desktop's Pane Manager lists). This pane keeps no copy it edits: a list cursor, held by
// identity, and the reading it was last told.
//
// THE COMPOSITION IS THE OBJECT INSPECTOR'S. Two headings, the max-min fair share of the body
// between the two lists, the windows and their counted omissions, the notice row in front: what
// changed is what the lists hold. The two object controls (`[ Create ]`, `[ Delete ]`) retired
// with the objects; a pane is launched and closed from the Pane Manager, not from here.
//
// (!) WHAT THE SEAM COSTS IS THE DRAFT'S CARET. `PaneContent` is rows and a caret is a
// `SurfaceTextRegion` fact a pane cannot send, so a maker typing a value sees the text and no
// insertion point -- the documented loss the project browser's authoring line carries too. The
// visible WINDOW still follows the caret, so a long value scrolls to where the maker is typing.

#include "info-pane/vocabulary.hpp"

#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/inspection_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "inventory_editor.hpp"
#include "workshop/setup_control.hpp"
#include "component/text_box.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace component = zengine::component;
namespace input = zengine::input;
namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace pane = zengine::info_pane;

using ws::InspectPaneRequested;
using ws::InventoryPane;
using ws::PaneActionRequested;
using ws::PaneActionRow;
using ws::PaneActions;
using ws::PaneCatalogRequested;
using ws::PaneCommitRequested;
using ws::PaneContent;
using ws::PaneInventory;
using ws::PaneInventoryRequested;
using ws::PaneKey;
using ws::v2::PaneOffered;
using ws::PaneDrop;
using ws::PaneOperationRequested;
using ws::PaneOperationAnswered;
using ws::PanePressed;
using ws::PaneRoom;
using ws::PaneSubjectActed;
using ws::PaneSubjectRequested;
using ws::PaneSubjectShown;
using ws::PaneTextInput;
using ws::ShownProperty;

/// WHO THIS PANE IS TALKING TO -- the host's office, spelled as a literal exactly as the
/// other pane weaves spell it.
constexpr const char* kWorkshopRole = "zengine.workshop";

// ---- The measurements the composition spends, carried from the host --------------------
//
// `fit`, `pad`, `omitted_text` and `admissible` are `workshop/pane_text.hpp`'s, the header every
// pane weave shares. `list_window` is the host's (`screen_gestures.cpp`) and `share_body_rows` was
// (`screen_info.cpp`, until its last consumer there retired), carried byte-for-byte because
// `screen.hpp` is the host's presentation and not a header a loaded image may include.

using zengine::workshop::pane_text::admissible;
using zengine::workshop::pane_text::drawable;
using zengine::workshop::pane_text::fit;
using zengine::workshop::pane_text::omitted_text;
using zengine::workshop::pane_text::pad;

constexpr std::int64_t kPropertyMarkCols = 1;  ///< the `>` that marks the cursor's row
constexpr std::int64_t kPropertyLabelCols = 9; ///< the label column a value is measured after

/// WHAT A WINDOW OVER A LIST LOOKS LIKE -- `screen.hpp`'s `ListWindow`, carried.
struct ListWindow {
    std::size_t first = 0;
    std::size_t count = 0;
    std::size_t before = 0;
    std::size_t after = 0;
};

/// THE THREE RULES, CARRIED (`screen_gestures.cpp`): a population that fits is shown whole,
/// the focused row is always in the window, and every omission is counted on its own side
/// and spends a row of the same budget.
ListWindow list_window(std::size_t total, std::size_t selected_at, std::size_t rows) {
    ListWindow w;
    if (total == 0 || rows == 0) {
        w.after = total;
        return w;
    }
    if (total <= rows) {
        w.count = total;
        return w;
    }
    if (rows < 3) {
        w.after = total;
        return w;
    }
    if (selected_at >= total) {
        selected_at = 0;
    }
    const std::size_t one_marker = rows - 1;
    if (selected_at < one_marker) {
        w.count = one_marker;
        w.after = total - w.count;
        return w;
    }
    const std::size_t tail = total - one_marker;
    if (selected_at >= tail) {
        w.first = tail;
        w.count = one_marker;
        w.before = tail;
        return w;
    }
    w.count = rows - 2;
    w.first = selected_at + 1 - w.count;
    w.before = w.first;
    w.after = total - w.first - w.count;
    return w;
}

struct BodyShare {
    std::size_t panes = 0;
    std::size_t properties = 0;
};

/// MAX-MIN FAIR, CARRIED (`screen_info.cpp`): each list gets what it needs, spare stays
/// spare, and what both cannot have is shared equally with an unneeded half going to the
/// other. 50/50 is a consequence of the rule and not the rule.
BodyShare share_body_rows(std::size_t budget, std::size_t want_panes,
                          std::size_t want_properties) {
    BodyShare s;
    if (budget == 0) {
        return s;
    }
    if (want_properties <= budget && want_panes <= budget - want_properties) {
        s.panes = want_panes;
        s.properties = want_properties;
        return s;
    }
    const std::size_t half = budget / 2;
    if (want_panes <= half) {
        s.panes = want_panes;
        s.properties = budget - s.panes;
    } else if (want_properties <= budget - half) {
        s.properties = want_properties;
        s.panes = budget - s.properties;
    } else {
        s.panes = half;
        s.properties = budget - half;
    }
    return s;
}

// ---- The sentences -------------------------------------------------------------------------

/// THE PANE'S OWN REFUSAL WHILE A DRAFT IS LIVE -- for every act the draft holds back: naming
/// another subject, from a press or from Return on the list.
constexpr const char* kFinishTheEdit = "finish the edit first -- commit it or cancel it";

/// A COMMIT ASKED FOR WHILE ONE IS UNANSWERED (`act`): not sent, and the draft and its edits stand.
/// It belongs to the unanswered commit, whose answer retires or replaces it (`answered_commit`).
constexpr const char* kCommitNotSent = "commit not sent -- an earlier commit is still unanswered";

/// ENDING A DRAFT WITH NO COMMIT UNANSWERED (`end_draft`). What the draft held is gone, and a write
/// an earlier commit made is not: neither sentence says whether anything was written. A draft is
/// abandoned when the host's picture names other rows (`shows_draft_subject`) -- another pane,
/// another desk put live, or the rows' layout changing -- so the sentence names all three.
constexpr const char* kCancelled = "edit cancelled -- unwritten changes discarded";
constexpr const char* kAbandoned =
    "edit abandoned -- the inspected pane, its desk or its rows changed; unwritten changes "
    "discarded";

/// ...AND WITH ONE UNANSWERED, which a draft's end cannot take back. A cancel moves no subject,
/// so its commit may still be written; an abandonment promises nothing about the one it leaves.
constexpr const char* kCancelledSent =
    "commit already sent -- the draft is closed, and it may still be written";
constexpr const char* kAbandonedSent =
    "commit already sent -- the draft ended: the inspected pane, its desk or its rows changed";

/// A COMMIT'S ACCOUNT OVER A DRAFT THAT DOES NOT HOLD EXACTLY WHAT IT SENT -- its own draft typed
/// into since, or a newer one -- and over no draft at all (`answered_commit`). A refusal is
/// followed by the owner's own words.
constexpr const char* kEarlierWritten = "earlier commit written -- later edits not sent";
constexpr const char* kEarlierRefused = "earlier commit refused -- ";
constexpr const char* kLateWritten = "commit written -- it was sent before the draft closed";
constexpr const char* kLateRefused = "commit refused -- ";

/// AN ASK THAT NEVER REACHED THE DOOR (WL-INFO-13): nothing queued at all (the send's ticket is
/// not valid), or queued and refused by Loom before the door's handler ran (`zen.DispatchRefused`,
/// whose reason follows in parentheses). Either way nothing was written or changed, the record is
/// released, and the draft and its text stand for the next Return.
constexpr const char* kCommitNotQueued = "commit not submitted -- nothing was queued or written";
constexpr const char* kCommitUndelivered = "commit not delivered -- nothing was written";
constexpr const char* kEarlierUndelivered = "earlier commit not delivered -- nothing was written";
constexpr const char* kActNotQueued = " not submitted -- nothing was queued";
constexpr const char* kActUndelivered = " not delivered -- nothing changed";

/// WHAT AN INSPECT IS CALLED in the two sentences above.
constexpr const char* kInspectAct = "inspect";

/// NOTHING INSPECTED YET, said where the properties would be and when the keys are sent there.
constexpr const char* kNoSubjectRow = "(no subject -- Return on a pane inspects it)";
constexpr const char* kNoSubject = "nothing is inspected yet -- Return on a pane inspects it";

/// A row this pane will not open a draft on: the host's own sentence, said before asking.
std::string not_authored(const std::string& label) {
    return label + " is not authored -- it is what the screen makes of the authored value";
}

/// ONE PANE OF THE LIST, AS A WORD: the three facts the host keeps apart, said apart.
std::string pane_state_word(const InventoryPane& p) {
    if (!p.available) {
        return "gone";
    }
    if (p.open) {
        return p.waiting ? "no room" : "open";
    }
    return "closed";
}

// =============================================================================
// The weave
// =============================================================================

class InfoPaneWeave
    : public loom::WeaveBase<
          InfoPaneWeave, pane::InfoPaneState,
          loom::Accept<ws::PaneCarryAnswered, ws::PaneResetRequested, PaneDrop, ws::PaneValueDrop, PaneOperationAnswered, zengine::inventory::InventoryEntry,
                       loom::Refused, loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed, PaneKey,
                       PaneTextInput, PaneActionRequested, PaneInventory, PaneSubjectShown,
                       PaneSubjectActed, loom::DispatchRefused, surface::ClipboardCopy,
                       surface::ClipboardText>,
          loom::Emit<ws::PaneValueCarryRequested, PaneOperationRequested, zengine::inventory::InventoryRead, zengine::inventory::InventoryAdd,
                     zengine::inventory::InventoryWrite, PaneOffered, PaneActions, PaneContent, PaneInventoryRequested,
                     PaneSubjectRequested, InspectPaneRequested, PaneCommitRequested,
                     surface::ClipboardCopy, surface::ClipboardTextRequested>> {
public:
    void on(const ws::PaneResetRequested& request, loom::Mail& mail) {
        if (request.pane != pane::kInfoPane || committing_.awaiting || acting_.awaiting || inventory_.busy()) {
            (void)mail.answer(loom::Refused{"Info reset needs its pane and no pending commit"}); return;
        }
        inventory_ = pane::InventoryEditor{};
        draft_ = Draft{}; paste_ = Paste{};
        state_.on_panes = true; notice_.clear();
        declare(mail); say(mail); (void)mail.answer(loom::Ack{});
    }
    void on(const ws::PaneValueDrop& drop, loom::Mail& mail) { open_inventory(drop, mail); }
    void on(const PaneDrop& drop, loom::Mail& mail) {
        open_inventory(drop, mail);
    }
    template<class Drop>
    void open_inventory(const Drop& drop, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || drop.pane != pane::kInfoPane) return;
        if (draft_.open || committing_.awaiting) {
            notice_ = "Finish the pane-property edit before inspecting an inventory entry";
        } else {
            inventory_.open(drop, mail, asked_);
            if (!inventory_.active) notice_ = inventory_.client.notice;
        }
        declare(mail); say(mail);
    }
    void on(const ws::PaneCarryAnswered& answer, loom::Mail& mail) {
        if (inventory_.hear(answer, mail)) { declare(mail); say(mail); }
    }
    void on(const PaneOperationAnswered& answer, loom::Mail& mail) {
        if (inventory_.hear(answer, mail)) { declare(mail); say(mail); }
    }
    void on(const zengine::inventory::InventoryEntry& answer, loom::Mail& mail) {
        if (inventory_.hear(answer, mail)) { declare(mail); say(mail); }
    }
    void on(const loom::Refused& answer, loom::Mail& mail) {
        if (inventory_.hear(answer, mail)) { declare(mail); say(mail); }
    }
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

    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != pane::kInfoPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        granted_ = true;
        say(mail);
    }

    /// THE ONE INVENTORY, AS THE HOST SAYS IT -- published when it changes, or answered to this
    /// image's arrival. Replaced whole; the list cursor is found again by identity.
    void on(const PaneInventory& said, loom::Mail& mail) {
        if (!from_workshop(mail)) {
            return; // a stranger's list of panes is not the host's inventory
        }
        panes_ = said.panes;
        panes_heard_ = true;
        find_list_cursor();
        say(mail);
    }

    /// WHAT THE SUBJECT LOOKS LIKE, SAID BY THE HOST, AND WHAT ITS ROWS ADDRESS. Replaced WHOLE,
    /// never merged -- published when it changes, or answered to this image's arrival.
    void on(const PaneSubjectShown& said, loom::Mail& mail) {
        if (!from_workshop(mail)) {
            return; // a stranger's opinion about a pane is not a reading of one
        }
        known_ = said;
        heard_ = true;
        // (!) A DRAFT WHOSE ROWS ARE NO LONGER SHOWN IS ABANDONED, and it is the one thing this
        // pane drops without being asked -- so it says so. The host names what the rows address:
        // another pane, another desk put live, or another layout of rows is another name, even
        // where `Width` sits on the same row with the same value. Carrying the draft onto
        // whatever took its place would ask the owner to write the maker's text into a
        // different property, and it would refuse.
        bool moved = false;
        if (draft_.open && !shows_draft_subject()) {
            end_draft(kAbandoned, kAbandonedSent);
            moved = true;
        }
        if (clamp_cursor()) {
            moved = true;
        }
        if (moved) {
            declare(mail);
        }
        say(mail);
    }

    /// WHAT THE OWNER MADE OF AN ASK THIS PANE IS WAITING ON, read against that ask. The
    /// correlation says WHICH request an answer is about; the record it matches says what the
    /// request was, and a commit's draft incarnation and sent text say whether the draft open now
    /// is the one that sent it and holds nothing it did not send.
    ///
    /// An accepted ask says nothing here -- the host says a write on the band, and a picture it
    /// changed arrives as `PaneSubjectShown`. A refusal is the owner's own words.
    void on(const PaneSubjectActed& said, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        if (committing_.awaiting && mail.correlation() == committing_.pending) {
            const SentCommit was = std::move(committing_);
            committing_ = SentCommit{};
            answered_commit(was, said, mail);
            return;
        }
        if (!acting_.awaiting || mail.correlation() != acting_.pending) {
            return;
        }
        acting_ = Asked{};
        // AN INSPECT CLOSES NO DRAFT: one asked before a draft opened may be answered while it is
        // open, and the picture that follows it is what decides whether the draft still stands.
        if (!said.accepted) {
            notice_ = said.refusal;
            say(mail);
        }
    }

    /// LOOM'S WORD THAT AN ASK OF THIS PANE'S NEVER REACHED THE DOOR'S HANDLER
    /// (`zen.DispatchRefused`; WL-INFO-13) -- an attestation, not an answer. Provenance first:
    /// the shape alone is ordinary speech anyone may send. Then the exact queued attempt, its
    /// correlation and what it asked, matched against the one record that can still be waiting on
    /// it (`refused_ask`), and only that record is released. A forged, late, duplicate or
    /// mismatched notice settles nothing; an ask the door received and never answered is not
    /// this, and stays awaited.
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (inventory_.hear(refused, mail)) { declare(mail); say(mail); return; }
        if (!mail.dispatch_refused()) {
            return;
        }
        if (refused_ask(committing_, refused, mail, PaneCommitRequested::zen_name,
                        PaneCommitRequested::zen_version)) {
            const SentCommit was = std::move(committing_);
            committing_ = SentCommit{};
            undelivered_commit(was, refused, mail);
            return;
        }
        if (refused_ask(acting_, refused, mail, InspectPaneRequested::zen_name,
                        InspectPaneRequested::zen_version)) {
            const std::string act = acting_.act;
            acting_ = Asked{};
            notice_ = act + kActUndelivered + " (" + refused.reason + ")";
            say(mail);
        }
    }

    /// A PRESS NAMES A ROW OF THIS PANE'S ROOM, and this pane knows which list that row is in
    /// because it composed them. A pane row inspects that pane; a property row moves the cursor.
    void on(const PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != pane::kInfoPane) {
            return;
        }
        if (inventory_.active) { inventory_.press(press.row); say(mail); return; }
        const Placed at = placed(press.row);
        if (at.what == Placed::kNothing) {
            return; // a heading, a marker, a blank row
        }
        // THE MAKER HAS ACTED, SO THE LAST ACT'S ANSWER IS SPENT -- and spent means gone from the
        // rows Workshop holds (`agents/panes.md`). An inspect is an ask whose answer is still on
        // its way, and an accepted inspect of the pane already inspected brings no new picture,
        // so when a notice stood and the act published nothing the rows are said here, once,
        // without it.
        const bool spent = !notice_.empty();
        const std::uint64_t published = published_;
        notice_.clear();
        press_placed(at, mail);
        if (spent && published_ == published) {
            say(mail);
        }
    }

    /// ONLY THE DRAFT READS RAW KEYS. Everything else this pane does arrives as a resolved
    /// id; a component's editing gestures are the component's, not the pane's commands.
    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || key.pane != pane::kInfoPane) {
            return;
        }
        if (inventory_.active) { inventory_.key(key); say(mail); return; }
        if (!draft_.open) {
            return;
        }
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (!draft_.line.consume(key.scancode, key.modifiers, clip_)) {
            return; // a key that means nothing to the line is no act: the notice stands, unsaid
        }
        notice_.clear();
        if (clip_.writes != copied_before) {
            mail.publish(surface::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_paste(mail);
        }
        say(mail);
    }

    void on(const PaneTextInput& typed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || typed.pane != pane::kInfoPane) {
            return;
        }
        if (inventory_.active) { inventory_.text(typed.text); say(mail); return; }
        if (!draft_.open || typed.text.empty() || !admissible(typed.text)) {
            return;
        }
        notice_.clear();
        draft_.line.type(typed.text);
        say(mail);
    }

    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || asked.pane != pane::kInfoPane) {
            return;
        }
        if (asked.id == "info.inventory" && !draft_.open && inventory_.has_entry()) {
            inventory_.active = true; declare(mail); say(mail); return;
        }
        if (inventory_.active) {
            inventory_.act(asked.id, mail, asked_); declare(mail); say(mail); return;
        }
        // THE MODE OWNS THE PANE'S ACTIONS FIRST, AND AN ID IT DOES NOT ANSWER TO IS NO ACT.
        // While a draft is open this pane declares two rows and no more -- but the declaration
        // and the keystroke race across two messages (Escape and Return in one poll resolve to a
        // cancel and a commit, and the commit arrives after the cancel closed the draft), so a
        // stale id, or one nobody declared, must mean nothing: no commit, and no notice spent.
        if (!answers(asked.id)) {
            return;
        }
        // THE MAKER HAS ACTED, SO THE LAST ACT'S ANSWER IS SPENT -- in the rows Workshop holds,
        // too: a commit whose answer is on its way, or an edit with nothing to edit, says nothing
        // of its own (`on(PanePressed)` says why).
        const bool spent = !notice_.empty();
        const std::uint64_t published = published_;
        notice_.clear();
        act(asked, mail);
        if (spent && published_ == published) {
            say(mail);
        }
    }

    void on(const surface::ClipboardCopy& said, loom::Mail&) {
        // WHAT THE PROCESS SAYS IT COPIED -- this pane's own copies included, which is why
        // it is a mirror rather than a second store: a paste answers with this.
        clip_.text = said.text;
    }

    void on(const surface::ClipboardText& a, loom::Mail& mail) {
        if (!mail.answers_ask() || !paste_.awaiting || mail.correlation() != paste_.pending) {
            return;
        }
        paste_.awaiting = false;
        if (!draft_.open || draft_.line.draft_epoch() != paste_.epoch) {
            return; // the draft that asked is over; a later one did not ask, and gets nothing
        }
        const std::string text = a.readable ? a.text : clip_.text;
        if (text.empty() || !admissible(text)) {
            return;
        }
        draft_.line.type(text);
        say(mail);
    }

private:
    /// THE HOST, SPEAKING AS ITS OFFICE OR ANSWERING THIS IMAGE'S OWN ASK. A host answer carries
    /// answer provenance, not office authorship (Loom ANS-01), and it can only come from the
    /// office this image asked.
    static bool from_workshop(const loom::Mail& mail) {
        return mail.authored_from_role(kWorkshopRole) || mail.answers_ask();
    }

    /// WHAT ONE DECLARED ACTION DOES, BY ID -- with the notice already spent
    /// (`on(PaneActionRequested)`), and only for an id `answers` admitted in this mode.
    void act(const PaneActionRequested& asked, loom::Mail& mail) {
        if (draft_.open) {
            if (asked.id == pane::kActionCommit) {
                if (committing_.awaiting) {
                    // ONE COMMIT OUTSTANDING AT A TIME. A second would hide the first's answer, or
                    // be answered first; so it is not sent, aloud, and nothing else is touched: the
                    // draft, its edits and the first commit's record stand, and Return sends the
                    // edits once the first is answered -- or once Loom says it never arrived
                    // (`on(DispatchRefused)`). No retry is queued, and delivered silence still
                    // waits.
                    notice_ = kCommitNotSent;
                    committing_.promise = notice_;
                    say(mail);
                    return;
                }
                ask_commit(mail);
            } else if (asked.id == pane::kActionCancel) {
                // ESCAPE ALWAYS ENDS THE DRAFT, AND SAYS WHETHER THAT WAS ALL IT ENDED.
                end_draft(kCancelled, kCancelledSent);
                declare(mail);
                say(mail);
            }
            return;
        }
        if (asked.id == pane::kActionSwitch) {
            if (state_.on_panes && known_.subject == 0) {
                notice_ = kNoSubject; // there are no rows to put the keys in yet
                say(mail);
                return;
            }
            state_.on_panes = !state_.on_panes;
            declare(mail); // Return and the arrows mean the other list's acts now
            say(mail);
            return;
        }
        if (asked.id == pane::kActionEdit) {
            if (state_.on_panes) {
                inspect_cursor(mail);
            } else {
                begin_draft(mail);
            }
            return;
        }
        const std::int64_t by = asked.id == pane::kActionUp ? -1 : +1;
        if (asked.id != pane::kActionUp && asked.id != pane::kActionDown) {
            return;
        }
        if (state_.on_panes) {
            step_list(by);
        } else {
            step_property(by);
        }
        say(mail);
    }

    // ---- Offering and declaring ---------------------------------------------------------

    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kInfoPaneRole)
            .send_to_role(kWorkshopRole,
                          PaneOffered{pane::kInfoPane, pane::kInfoPaneName,
                                      pane::kInfoPaneSummary, 13, 58});
        declare(mail);
        // ...AND WHAT THE HOST HOLDS NOW: the inventory, and the subject this office named --
        // which a reload of this image finds standing, because the host keeps it. Both are
        // published when they change; an image arriving while nothing changes would otherwise
        // wait for an unrelated change.
        (void)mail.as_role(pane::kInfoPaneRole)
            .send_to_role(kWorkshopRole, PaneInventoryRequested{});
        (void)mail.as_role(pane::kInfoPaneRole)
            .send_to_role(kWorkshopRole, PaneSubjectRequested{});
    }

    /// WHAT THIS PANE ANSWERS TO RIGHT NOW -- re-declared whenever the mode changes.
    ///
    /// A PANE IS ONE KEYBOARD CONTEXT, AND A MODE IS NOT A SECOND ONE (WL-FILES-16). So while a
    /// maker is typing a value this pane declares two rows and no more, and every other key
    /// reaches it as an ordinary `PaneKey` for the line to consume -- which is what lets
    /// Backspace delete a character rather than meaning anything of the pane's.
    void declare(loom::Mail& mail) {
        PaneActions actions;
        actions.pane = pane::kInfoPane;
        actions.rows = action_rows();
        (void)mail.as_role(pane::kInfoPaneRole).send_to_role(kWorkshopRole, actions);
    }

    /// THE ROWS OF THE MODE THIS PANE IS IN -- what `declare` tells Workshop, and what `answers`
    /// reads, so what the pane acts on and what it said it acts on are one list. The ids do not
    /// change between the two lists, only what their labels say they do.
    std::vector<PaneActionRow> action_rows() const {
        if (inventory_.active) return inventory_.actions();
        std::vector<PaneActionRow> rows;
        const auto row = [&rows](const char* id, const char* label, std::int64_t sc,
                                 std::int64_t mods = input::mod::kNone) {
            rows.push_back(PaneActionRow{id, label, sc, mods});
        };
        if (draft_.open) {
            row(pane::kActionCommit, "commit", input::scan::kReturn);
            row(pane::kActionCancel, "cancel", input::scan::kEscape);
        } else if (state_.on_panes) {
            row(pane::kActionUp, "pane up", input::scan::kUp);
            row(pane::kActionDown, "pane down", input::scan::kDown);
            row(pane::kActionEdit, "inspect", input::scan::kReturn);
            row(pane::kActionSwitch, "properties", input::scan::kTab);
        } else {
            row(pane::kActionUp, "row up", input::scan::kUp);
            row(pane::kActionDown, "row down", input::scan::kDown);
            row(pane::kActionEdit, "edit", input::scan::kReturn);
            row(pane::kActionSwitch, "panes", input::scan::kTab);
        }
        if (!draft_.open && inventory_.has_entry()) rows.push_back({"info.inventory", "inventory entry",
            input::scan::kI, input::mod::kCtrl});
        return rows;
    }

    bool answers(const std::string& id) const {
        for (const PaneActionRow& row : action_rows()) {
            if (row.id == id) {
                return true;
            }
        }
        return false;
    }

    // ---- The list cursor, held by identity --------------------------------------------------

    /// FIND THE PANE THE LIST CURSOR HOLDS in the list as the host just said it. By identity, so
    /// a row inserted above it moves the marker with it, and one that returns is found again.
    ///
    /// (!) A CHOICE WHOSE ROW LEFT IS STILL A CHOICE, AND ITS ABSENCE IS STATE (WL-DESK-10's
    /// rule, one pane over). The keys stay in `InfoPaneState`; the marker holds nothing and says
    /// so, and Return inspects nothing until the maker chooses a row -- in this image and in every
    /// image a reload hands the state to. Only a cursor never given a pane (both keys empty) takes
    /// the row it stands on. Clearing the keys once made a reloaded Info read a lost choice as
    /// none, hold the first row, and inspect it on the next Return.
    void find_list_cursor() {
        const std::int64_t n = static_cast<std::int64_t>(panes_.size());
        const bool chosen = !state_.list_office.empty() || !state_.list_pane.empty();
        for (std::int64_t i = 0; chosen && i < n; ++i) {
            const InventoryPane& p = panes_[static_cast<std::size_t>(i)];
            if (p.office == state_.list_office && p.pane == state_.list_pane) {
                list_at_ = i;
                held_name_ = p.name;
                lost_ = false;
                return;
            }
        }
        lost_ = chosen;
        if (list_at_ >= n) {
            list_at_ = n > 0 ? n - 1 : 0;
        }
        if (list_at_ < 0) {
            list_at_ = 0;
        }
        if (!chosen && n > 0) {
            hold_list(list_at_);
        }
    }

    void hold_list(std::int64_t at) {
        const InventoryPane& p = panes_[static_cast<std::size_t>(at)];
        list_at_ = at;
        state_.list_office = p.office;
        state_.list_pane = p.pane;
        held_name_ = p.name;
        lost_ = false;
    }

    void step_list(std::int64_t by) {
        const std::int64_t n = static_cast<std::int64_t>(panes_.size());
        if (n == 0) {
            return;
        }
        std::int64_t at = list_at_ + by;
        at = at < 0 ? 0 : (at >= n ? n - 1 : at);
        hold_list(at);
    }

    /// THE PROPERTY CURSOR MOVES OVER ROWS AND STEPS OVER SECTION HEADINGS, which hold no value.
    void step_property(std::int64_t by) {
        const std::int64_t n = static_cast<std::int64_t>(known_.properties.size());
        std::int64_t at = state_.cursor;
        while (true) {
            at += by;
            if (at < 0 || at >= n) {
                return; // at the edge: nothing moved
            }
            if (!known_.properties[static_cast<std::size_t>(at)].section) {
                state_.cursor = at;
                return;
            }
        }
    }

    // ---- The asks -----------------------------------------------------------------------

    /// AN ASK THIS PANE IS WAITING ON (`ask_inspect`, `ask_commit`).
    struct Asked {
        bool awaiting = false;
        std::uint64_t pending = 0; ///< the correlation: which request an answer names
        /// THE QUEUED ATTEMPT, Loom's sequence for this send: what `zen.DispatchRefused` names when
        /// the send never reached the door's handler. It proves the send was queued and no more.
        loom::Ticket attempt{};
        std::string act; ///< what it asked for, for the sentence a refusal of the send needs
    };

    /// ...AND A COMMIT, read against the draft that sent it and what it sent. A draft incarnation
    /// is not its contents: typing changes the text and keeps the epoch, which is why a paste may
    /// land in a draft that has moved on and why a write does not cover what was typed after
    /// Return. The subject is the name the draft was opened on (`Draft::subject`), and the host
    /// writes the commit only while that name is still its rows' own.
    struct SentCommit : Asked {
        std::uint64_t draft = 0; ///< `TextBox::draft_epoch` when it was sent: which draft
        std::string text;        ///< ...and what it sent: whether that draft holds anything more
        std::string promise;     ///< the sentence said about it while it was unanswered
    };

    /// NAME A PANE AS THE SUBJECT -- the list cursor's, or a pressed row's. Refused here, before
    /// anything is asked, while a draft is live: another subject would take the draft's rows away.
    void inspect_cursor(loom::Mail& mail) {
        if (panes_.empty()) {
            return;
        }
        if (lost_ || (state_.list_office.empty() && state_.list_pane.empty())) {
            notice_ = "Return inspected nothing -- choose a row first";
            say(mail);
            return;
        }
        ask_inspect(state_.list_office, state_.list_pane, mail);
    }

    /// ONE INSPECT OUTSTANDING, THE NEWEST: an older one's answer names a correlation nothing
    /// waits on and is dropped unread. The commit's record is never replaced by it.
    ///
    /// (!) THE TICKET IS KEPT (WL-INFO-13). One that is not valid means nothing was queued: no
    /// answer and no refusal notice can follow, so the record is released at once and the pane
    /// says so.
    void ask_inspect(const std::string& office, const std::string& pane_key, loom::Mail& mail) {
        const std::uint64_t correlation = ++asked_;
        Asked asking;
        asking.awaiting = true;
        asking.pending = correlation;
        asking.act = kInspectAct;
        asking.attempt =
            mail.as_role(pane::kInfoPaneRole)
                .send_to_role(kWorkshopRole, InspectPaneRequested{office, pane_key}, correlation);
        if (!asking.attempt.valid()) {
            acting_ = Asked{};
            notice_ = std::string(kInspectAct) + kActNotQueued;
            say(mail);
            return;
        }
        acting_ = std::move(asking);
        say(mail); // the list cursor moved; the picture the answer brings says the rest
    }

    /// DOES THIS NOTICE NAME THE ASK `record` IS WAITING ON -- the same queued attempt, under the
    /// same correlation, of the shape it sent, to the office it addressed? Every half, because a
    /// sequence alone is a number and the rest is what this pane asked.
    static bool refused_ask(const Asked& record, const loom::DispatchRefused& refused,
                            const loom::Mail& mail, const char* shape, std::uint32_t version) {
        const loom::Ticket attempt = refused.refused_attempt();
        return record.awaiting && record.attempt.valid() && attempt.valid() &&
               attempt.seq == record.attempt.seq && mail.correlation() == record.pending &&
               refused.shape == shape && refused.version == static_cast<std::int64_t>(version) &&
               refused.role == kWorkshopRole && refused.target.empty();
    }

    /// A COMMIT'S ANSWER -- about the draft that sent it and what it sent, and about nothing else.
    ///
    /// It is said where the notice row holds this commit's own sentence (`SentCommit::promise`),
    /// or is empty while the draft that sent it is open; a sentence a later act said stands.
    void answered_commit(const SentCommit& was, const PaneSubjectActed& said, loom::Mail& mail) {
        const bool own = !was.promise.empty() && notice_ == was.promise;
        const bool same_draft = draft_.open && draft_.line.draft_epoch() == was.draft;
        const bool may_say = own || (same_draft && notice_.empty());
        if (same_draft && draft_.line.text() == was.text) {
            // THE DRAFT HOLDS EXACTLY WHAT IT SENT: accepted ends it, and a sentence about the
            // commit pending goes with it; a refusal stands beside it in the owner's words.
            if (said.accepted) {
                close_draft();
                declare(mail);
                if (own) {
                    notice_.clear();
                }
                say(mail);
            } else if (may_say) {
                notice_ = said.refusal;
                say(mail);
            }
            return;
        }
        // (!) ANY OTHER DRAFT IS EDITS NO WRITE COVERS -- this one typed into after Return, or a
        // newer one -- so the answer closes, alters and marks none of them, and says it as the
        // earlier commit's. With no draft open, the draft that sent it is over: the write was
        // never the draft's to take back, and the account replaces only the sentence that
        // promised it.
        if (!may_say) {
            return;
        }
        if (draft_.open) {
            notice_ = said.accepted ? std::string(kEarlierWritten)
                                    : std::string(kEarlierRefused) + said.refusal;
        } else {
            notice_ = said.accepted ? std::string(kLateWritten)
                                    : std::string(kLateRefused) + said.refusal;
        }
        say(mail);
    }

    /// A COMMIT NAMES THE ROWS ITS DRAFT WAS OPENED ON, THE ROW AND THE TEXT, and keeps its ticket
    /// as an inspect does: nothing queued releases the record at once, and the draft and its text
    /// stand, so the next Return is a fresh attempt rather than a second commit.
    void ask_commit(loom::Mail& mail) {
        PaneCommitRequested request;
        request.subject = draft_.subject;
        request.row = draft_.row;
        request.text = draft_.line.text();
        const std::uint64_t correlation = ++asked_;
        SentCommit sending;
        sending.awaiting = true;
        sending.pending = correlation;
        sending.act = "commit";
        sending.draft = draft_.line.draft_epoch();
        sending.text = request.text;
        sending.attempt = mail.as_role(pane::kInfoPaneRole)
                              .send_to_role(kWorkshopRole, std::move(request), correlation);
        if (!sending.attempt.valid()) {
            committing_ = SentCommit{};
            notice_ = kCommitNotQueued;
            say(mail);
            return;
        }
        committing_ = std::move(sending);
    }

    /// A COMMIT LOOM REFUSED BEFORE THE DOOR RAN -- nothing was written, so it closes no draft: the
    /// one that sent it keeps its text and its history, and Return sends what it holds. The account
    /// is said where an answer's would be (`answered_commit`): over this commit's own sentence, or
    /// over an empty row while its own draft is open; a sentence a later act said stands.
    void undelivered_commit(const SentCommit& was, const loom::DispatchRefused& refused,
                            loom::Mail& mail) {
        const bool own = !was.promise.empty() && notice_ == was.promise;
        const bool same_draft = draft_.open && draft_.line.draft_epoch() == was.draft;
        if (!own && !(same_draft && notice_.empty())) {
            return;
        }
        const std::string why = " (" + refused.reason + ")";
        notice_ = (draft_.open && !same_draft ? std::string(kEarlierUndelivered)
                                              : std::string(kCommitUndelivered)) +
                  why;
        say(mail);
    }

    void begin_paste(loom::Mail& mail) {
        paste_.pending = ++asked_;
        paste_.epoch = draft_.line.draft_epoch();
        paste_.awaiting = true;
        (void)mail.as_role(pane::kInfoPaneRole)
            .send_to_role(surface::kSkinRole, surface::ClipboardTextRequested{},
                          paste_.pending);
    }

    // ---- The draft ----------------------------------------------------------------------

    void begin_draft(loom::Mail& mail) {
        if (known_.subject == 0) {
            notice_ = kNoSubject;
            say(mail);
            return;
        }
        if (state_.cursor < 0 ||
            static_cast<std::size_t>(state_.cursor) >= known_.properties.size()) {
            return;
        }
        const ShownProperty& row = known_.properties[static_cast<std::size_t>(state_.cursor)];
        if (!row.editable) {
            notice_ = not_authored(row.label);
            say(mail);
            return;
        }
        draft_.open = true;
        draft_.row = state_.cursor;
        draft_.label = row.label;
        draft_.subject = known_.subject;
        draft_.line.set(row.value, row.value.size()); // the caret at the end, as it was
        declare(mail);
        say(mail);
    }

    /// `clear` ends the line's draft incarnation (`component::TextBox::draft_epoch`), which is
    /// what a commit or a paste that the draft sent is read against when its answer comes back;
    /// it is never called to make a fresh epoch for a draft that stays open.
    void close_draft() {
        draft_.open = false;
        draft_.row = 0;
        draft_.label.clear();
        draft_.subject = 0;
        draft_.line.clear();
    }

    /// END THE LIVE DRAFT AND SAY WHAT ENDING IT DID: `ended` when no commit is unanswered,
    /// `sent` while one is -- this draft's, or one an earlier draft sent. Closing a draft is
    /// always this pane's to do; a write already asked for is not, so the second sentence
    /// promises neither outcome, and the commit's record keeps it as the one sentence its
    /// answer's account may replace (`answered_commit`). No commit unanswered is not proof that
    /// nothing was written -- an earlier one may have been taken -- so `ended` says only that
    /// what was never written is gone.
    void end_draft(const char* ended, const char* sent) {
        close_draft();
        notice_ = committing_.awaiting ? sent : ended;
        if (committing_.awaiting) {
            committing_.promise = notice_;
        }
    }

    /// DOES THE PICTURE STILL SHOW THE PROPERTY THE DRAFT IS TYPED INTO -- the rows the host
    /// named when the draft opened, with the same label on the draft's row. The same row of
    /// another pane, or of the same pane on another desk, is another property: the host names it
    /// anew.
    bool shows_draft_subject() const {
        return known_.subject == draft_.subject &&
               draft_.row < static_cast<std::int64_t>(known_.properties.size()) &&
               known_.properties[static_cast<std::size_t>(draft_.row)].label == draft_.label;
    }

    /// KEEP THE PROPERTY CURSOR ON A ROW THAT EXISTS AND HOLDS A VALUE, and the keys on the pane
    /// list while there are no rows to hold them. Says whether the keys moved lists, which is a
    /// change of what Return and the arrows mean -- so the caller declares again.
    bool clamp_cursor() {
        const std::int64_t total = static_cast<std::int64_t>(known_.properties.size());
        if (state_.cursor >= total) {
            state_.cursor = total == 0 ? 0 : total - 1;
        }
        if (state_.cursor < 0) {
            state_.cursor = 0;
        }
        if (total > 0 && known_.properties[static_cast<std::size_t>(state_.cursor)].section) {
            step_property(+1);
        }
        if ((total == 0 || known_.subject == 0) && !state_.on_panes) {
            state_.on_panes = true; // no rows to hold the keys
            return true;
        }
        return false;
    }

    // ---- Where a press lands -------------------------------------------------------------

    /// WHAT A ROW OF THIS PANE'S ROOM IS. Composed once by `say` and inverted here, so a
    /// press cannot land where a row is not -- the one-geometry rule, on the pane's side of
    /// the seam now that there is no shared painter to ask.
    struct Placed {
        enum What { kNothing, kPane, kProperty };
        What what = kNothing;
        std::size_t index = 0;
    };

    Placed placed(std::int64_t row) const {
        for (const Row& r : composed_) {
            if (r.row == row) {
                return Placed{r.what, r.index};
            }
        }
        return Placed{};
    }

    /// WHAT A PRESS ON A PLACED ROW DOES -- with the notice already spent (`on(PanePressed)`).
    void press_placed(const Placed& at, loom::Mail& mail) {
        // A COMPOSITION OLDER THAN THE READING: a list that shrank since the rows were said (a
        // room of zero says nothing and so re-composes nothing). The press names no row now.
        if ((at.what == Placed::kPane && at.index >= panes_.size()) ||
            (at.what == Placed::kProperty && at.index >= known_.properties.size())) {
            return;
        }
        if (at.what == Placed::kPane) {
            if (draft_.open) {
                // ANOTHER SUBJECT TAKES THE ROWS A DRAFT IS TYPED INTO, the one already inspected
                // included, so a live draft holds it back: refused here, before anything is
                // asked, and the draft is exactly what it was.
                notice_ = kFinishTheEdit;
                say(mail);
                return;
            }
            hold_list(static_cast<std::int64_t>(at.index));
            if (!state_.on_panes) {
                state_.on_panes = true;
                declare(mail);
            }
            ask_inspect(state_.list_office, state_.list_pane, mail);
        } else if (at.what == Placed::kProperty) {
            if (known_.properties[at.index].section) {
                return;
            }
            state_.cursor = static_cast<std::int64_t>(at.index);
            if (state_.on_panes && !draft_.open) {
                state_.on_panes = false;
                declare(mail);
            }
            say(mail);
        }
    }

    // ---- Saying what the pane shows -------------------------------------------------------

    void say(loom::Mail& mail) {
        if (inventory_.active) {
            if (granted_ && rows_ > 0 && columns_ > 0) {
                ++published_;
                mail.as_role(pane::kInfoPaneRole).send_to_role(kWorkshopRole,
                    PaneContent{pane::kInfoPane, inventory_.draw(rows_, columns_)});
            }
            return;
        }
        if (!granted_ || rows_ <= 0 || columns_ <= 0) {
            return;
        }
        std::vector<surface::SurfaceTextRow> out;
        composed_.clear();
        const auto push = [&out, this](std::string text, std::int64_t role,
                                       std::int64_t ground = surface::role::kNone) {
            out.push_back(
                surface::SurfaceTextRow{drawable(fit(std::move(text), columns_)), role, ground});
        };
        // THE BODY IS COMPOSED WITHOUT THE NOTICE, and `finish` puts it in front, so a row's
        // published place is where it lands in `out` plus `lead()`.
        const auto mark = [&out, this](Placed::What what, std::size_t index) {
            composed_.push_back(Row{static_cast<std::int64_t>(out.size()) + lead(), what, index});
        };
        if (!panes_heard_) {
            push("PANES (waiting)", surface::role::kMuted);
            finish(out, mail);
            return;
        }
        // THE BUDGET, TAKEN BEFORE EITHER LIST IS OFFERED ANYTHING: two headings, and the front
        // row when there is one -- a bound that grows when it is exceeded is not a bound.
        const std::int64_t reserved = 2 + (front_sentence().empty() ? 0 : 1);
        const std::int64_t budget = rows_ - reserved;
        push("PANES -- " + std::to_string(panes_.size()), surface::role::kAccent);
        if (budget <= 0) {
            finish(out, mail); // room for the heading and nothing else: no invented room
            return;
        }
        const std::size_t want_properties =
            known_.subject == 0 ? 1 : known_.properties.size(); // the sentence wants its row
        const BodyShare share =
            share_body_rows(static_cast<std::size_t>(budget), panes_.size(), want_properties);
        say_panes(share.panes, push, mark);
        while (out.size() < 1 + share.panes) {
            push(std::string(), surface::role::kFill);
        }
        // A SECTION BEGINS HERE, AND THE GROUND IS WHAT SAYS SO: accent ink alone is not enough
        // when the row above is the accent-marked subject.
        push(known_.subject == 0 ? std::string("PROPERTIES") : "PANE " + known_.name,
             surface::role::kAccent, surface::role::kMuted);
        say_properties(share.properties, push, mark);
        finish(out, mail);
    }

    /// THE SENTENCE IN FRONT OF THE BODY: the notice when one stands, else the list cursor holding
    /// nothing -- said for as long as it holds nothing, a state and not an event, so no act spends
    /// it and a later notice only covers it while that notice stands.
    std::string front_sentence() const {
        if (!notice_.empty()) {
            return notice_;
        }
        if (lost_) {
            // THE NAME THIS IMAGE LAST SAW ON THE ROW, or -- an image that never saw it -- its key.
            return (held_name_.empty() ? state_.list_pane : held_name_) +
                   " left the list -- choose a row before Return inspects anything";
        }
        return std::string();
    }

    /// HOW MANY ROWS `finish` PUTS IN FRONT OF THE BODY: the front sentence, when there is one and
    /// a room to hold it beside the body. The press inverse adds the same number, so a sentence
    /// cannot move a press off its row.
    std::int64_t lead() const { return !front_sentence().empty() && rows_ > 1 ? 1 : 0; }

    template <class Push, class Mark>
    void say_panes(std::size_t share, Push&& push, Mark&& mark) {
        if (panes_.empty()) {
            (void)share; // said whatever the share is: an empty list is offered nothing
            push("(no panes -- nothing is offered or on the desk)", surface::role::kMuted);
            return;
        }
        const ListWindow win =
            list_window(panes_.size(), static_cast<std::size_t>(list_at_), share);
        if (win.before > 0) {
            push(omitted_text(win.before, "earlier"), surface::role::kMuted);
        }
        for (std::size_t n = 0; n < win.count; ++n) {
            const std::size_t i = win.first + n;
            const InventoryPane& p = panes_[i];
            const bool here = static_cast<std::int64_t>(i) == list_at_;
            const bool subject = p.office == known_.office && p.pane == known_.pane &&
                                 !known_.office.empty();
            // `>` WHERE THE KEYS' CURSOR IS, `?` WHERE IT HOLDS NOTHING, `*` ON THE SUBJECT.
            const char* marker = here && state_.on_panes ? (lost_ ? "?" : ">") : " ";
            std::int64_t role = surface::role::kFill;
            if (subject || (here && state_.on_panes)) {
                role = surface::role::kAccent;
            } else if (!p.available) {
                role = surface::role::kMuted;
            }
            mark(Placed::kPane, i);
            push(std::string(marker) + (subject ? "*" : " ") + p.name + " -- " +
                     pane_state_word(p),
                 role);
        }
        if (win.after > 0) {
            push(omitted_text(win.after, "more"), surface::role::kMuted);
        }
    }

    template <class Push, class Mark>
    void say_properties(std::size_t share, Push&& push, Mark&& mark) {
        if (known_.subject == 0) {
            (void)share; // said whatever the share is, for `say_panes`' reason
            push(kNoSubjectRow, surface::role::kMuted);
            return;
        }
        const std::size_t focus =
            draft_.open ? static_cast<std::size_t>(draft_.row)
                        : static_cast<std::size_t>(state_.cursor < 0 ? 0 : state_.cursor);
        const ListWindow win = list_window(known_.properties.size(), focus, share);
        if (win.before > 0) {
            push(omitted_text(win.before, "earlier"), surface::role::kMuted);
        }
        const std::int64_t value_columns = columns_ - kPropertyMarkCols - kPropertyLabelCols;
        for (std::size_t n = 0; n < win.count; ++n) {
            const std::size_t i = win.first + n;
            const ShownProperty& p = known_.properties[i];
            if (p.section) {
                push(" " + p.label, surface::role::kAccent);
                continue;
            }
            const bool here = i == focus && !state_.on_panes;
            const bool editing = draft_.open && i == static_cast<std::size_t>(draft_.row);
            std::int64_t role = surface::role::kFill;
            if (editing) {
                role = surface::role::kAlert; // a live draft is never quiet
            } else if (!p.editable) {
                role = surface::role::kMuted; // not the maker's to author
            }
            std::string text = std::string(here || editing ? ">" : " ") +
                               pad(p.label, static_cast<std::size_t>(kPropertyLabelCols));
            if (editing) {
                // THE WINDOW STILL FOLLOWS THE CARET even though the caret cannot cross, so
                // a long value scrolls to where the maker is typing.
                draft_.line.keep_caret_visible(value_columns);
                text += draft_.line.visible(value_columns);
            } else {
                text += fit(p.value, value_columns);
            }
            mark(Placed::kProperty, i);
            push(std::move(text), role);
        }
        if (win.after > 0) {
            push(omitted_text(win.after, "more"), surface::role::kMuted);
        }
    }

    void finish(std::vector<surface::SurfaceTextRow>& out, loom::Mail& mail) {
        if (lead() > 0) {
            if (static_cast<std::int64_t>(out.size()) > rows_ - 1) {
                out.resize(static_cast<std::size_t>(rows_ - 1));
            }
            out.insert(out.begin(),
                       surface::SurfaceTextRow{drawable(fit(front_sentence(), columns_)),
                                               surface::role::kAlert});
        }
        if (static_cast<std::int64_t>(out.size()) > rows_) {
            out.resize(static_cast<std::size_t>(rows_));
        }
        ++published_;
        (void)mail.as_role(pane::kInfoPaneRole)
            .send_to_role(kWorkshopRole, PaneContent{pane::kInfoPane, std::move(out)});
    }

    // ---- State not in the shape ------------------------------------------------------------

    struct Row {
        std::int64_t row = 0;
        Placed::What what = Placed::kNothing;
        std::size_t index = 0;
    };

    struct Draft {
        bool open = false;
        std::int64_t row = 0;
        std::string label;        ///< what the row was called when the draft opened
        std::int64_t subject = 0; ///< ...and what the host named those rows: what a commit returns
        component::TextBox line;
    };

    struct Paste {
        bool awaiting = false;
        std::uint64_t pending = 0;
        /// THE DRAFT THAT ASKED (`component::TextBox::draft_epoch`): the correlation says the
        /// answer is to this pane's ask, and the epoch says the line it was asked for still
        /// stands.
        std::uint64_t epoch = 0;
    };

    pane::InventoryEditor inventory_;
    zengine::ActivationCursor activation_;

    /// THE HOST'S LAST READING OF THE INVENTORY, and where in it the list cursor stands.
    std::vector<InventoryPane> panes_;
    bool panes_heard_ = false;
    std::int64_t list_at_ = 0;
    /// THE CHOSEN PANE IS NOT IN THE LIST -- derived from `InfoPaneState`'s keys by
    /// `find_list_cursor` whenever the list is said, never kept apart from them.
    bool lost_ = false;
    std::string held_name_;

    /// THE HOST'S LAST READING OF THE SUBJECT, and the name of what its rows address. Replaced
    /// whole; owned by nobody here; deliberately NOT in the state shape.
    PaneSubjectShown known_;
    bool heard_ = false;

    Draft draft_;
    Paste paste_;
    component::Clipboard clip_;
    std::vector<Row> composed_;
    std::string notice_;
    /// HOW MANY TIMES THIS PANE HAS SAID ITS ROWS -- how a handler that spent a notice learns
    /// whether the act it ran said them without it (`on(PanePressed)`).
    std::uint64_t published_ = 0;

    /// ONE COUNTER FOR EVERY QUESTION THIS PANE ASKS, so a correlation is this incarnation's
    /// own and an answer to somebody else's question is not mistaken for one to ours.
    std::uint64_t asked_ = 0;
    Asked acting_;          ///< an inspect
    SentCommit committing_; ///< a commit

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(InfoPaneWeave)
