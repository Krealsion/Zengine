// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Info pane -- a loadable weave that offers Workshop one pane: the OBJECTS a maker has
// authored and the PROPERTIES of the one they are looking at.
//
// IT USED TO BE C++ INSIDE THE HOST (`workshop/panel.hpp`'s `panel::kInfo`,
// `screen_info.cpp`'s `paint_info` and its eighteen helpers, `Session::rows`,
// `Session::cursor`, five `kActionCatalog` rows and the whole `KeyContext::kDraft` arm for
// property drafts). Now it is a weave beside the Skin, the Timer, the browser, the Builder
// and Attention.
//
// ⚠ THE DOCUMENT DID NOT COME WITH IT, AND THAT IS THE MIGRATION'S DECISION.
// `WorkshopDoc` is the HOST's -- the workspace plane paints it, the pointer drags it, `^s`
// writes it, the status slot counts it, a restore replaces it. This pane is one reader of it
// and the one that shows it as a list. So what crosses is a PICTURE the host derives
// (`DocumentShown`), and the four things a maker does here cross back as one ask
// (`DocumentActRequested`). Nothing in this image can touch a document; it can ask, and be
// refused in the document's own words.
//
// ⚠ AND THE PICTURE IS PUBLISHED RATHER THAN ASKED FOR, which is this migration's one new
// sentence. WL-DOC-14 requires the canvas, the object list and the inspector to agree after
// every gesture, and the document changes under this pane constantly with no gesture into it
// at all -- a drag on the workspace, a nudge, a create, a restore, a workspace refit. A pane
// that could only ask would be a list that is wrong most of the time.
//
// THE COMPOSITION DID NOT MOVE ITS MEANING. The two headings, the max-min fair share of the
// body between the two lists, the windows, the omission markers, the padded shares, the
// bracketed controls and their availability are `paint_info`'s, carried here: what changed is
// that the rows are SAID as values into a room this pane is granted, instead of being written
// into a region this pane resolved for itself.
//
// ⚠ WHAT THE MOVE COSTS IS THE DRAFT'S CARET. `PaneContent` is rows and a caret is a
// `SurfaceTextRegion` fact a pane cannot send, so a maker typing a property value sees the
// text and no insertion point -- the same documented loss the project browser's authoring
// line and the Powers query already carry, and the reason the Editor's migration is the
// contract that moves a caret across this seam. The visible WINDOW still follows the caret,
// so a long value scrolls to where the maker is typing.

#include "info-pane/vocabulary.hpp"

#include "workshop/document_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
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

using ws::DocumentActed;
using ws::DocumentActRequested;
using ws::DocumentShown;
using ws::PaneActionRequested;
using ws::PaneActionRow;
using ws::PaneActions;
using ws::PaneCatalogRequested;
using ws::PaneContent;
using ws::PaneKey;
using ws::PaneOffered;
using ws::PanePressed;
using ws::PaneRoom;
using ws::PaneTextInput;
using ws::ShownObject;
using ws::ShownProperty;

/// WHO THIS PANE IS TALKING TO -- the host's office, spelled as a literal exactly as the
/// other three pane weaves spell it.
constexpr const char* kWorkshopRole = "zengine.workshop";

// ---- The measurements the composition spends, carried from the host --------------------
//
// `detail::fit`, `detail::pad`, `omitted_text`, `list_window` and `share_body_rows` are
// Workshop's own (`screen_bindings.cpp`, `screen_gestures.cpp`, `screen_info.cpp`) and they
// live behind `screen.hpp`, which is the host's presentation and not a header a loaded image
// may include. They are carried here byte-for-byte rather than approximated, because the
// composition below is a MOVE: a panel that cut its rows one character differently after the
// migration would be a panel a maker could see had changed, for no reason they were told
// about.
//
// ⚠ THE FOURTH COPY OF `fit` IS GONE, AND SO ARE THE OTHER THREE. This file's first stage said
// it was the fourth package to carry these and that a header owning no lifecycle and hiding no
// bus would hold them honestly; `workshop/pane_text.hpp` is that header. It arrived when the
// Terminal would have made the fifth copy, and nothing about the functions changed for moving.
// `list_window` stays here: two packages carry it, it is a WINDOW rather than text, and two is
// still a convention.

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
    std::size_t objects = 0;
    std::size_t properties = 0;
};

/// MAX-MIN FAIR, CARRIED (`screen_info.cpp`): each list gets what it needs, spare stays
/// spare, and what both cannot have is shared equally with an unneeded half going to the
/// other. 50/50 is a consequence of the rule and not the rule.
BodyShare share_body_rows(std::size_t budget, std::size_t want_objects,
                          std::size_t want_properties) {
    BodyShare s;
    if (budget == 0) {
        return s;
    }
    if (want_properties <= budget && want_objects <= budget - want_properties) {
        s.objects = want_objects;
        s.properties = want_properties;
        return s;
    }
    const std::size_t half = budget / 2;
    if (want_objects <= half) {
        s.objects = want_objects;
        s.properties = budget - s.objects;
    } else if (want_properties <= budget - half) {
        s.properties = want_properties;
        s.objects = budget - s.properties;
    } else {
        s.objects = half;
        s.properties = budget - half;
    }
    return s;
}

// ---- The two controls ------------------------------------------------------------------

constexpr std::size_t kActionCreate = 0;
constexpr std::size_t kActionDelete = 1;
constexpr std::size_t kActionCount = 2;

/// WHY AN ACTION CANNOT RUN RIGHT NOW, or that it can -- `screen.hpp`'s own three, carried
/// with the constexpr rule that decides between them. Both facts the rule needs are this
/// pane's to know: whether ITS draft is live, and whether the picture it was shown has a
/// selection.
enum class Availability { kAvailable, kNoTarget, kDraftLive };

/// THE PANE'S OWN REFUSAL WHILE A DRAFT IS LIVE -- one sentence for every act the draft holds
/// back: the two controls, and a press on an object (`press_placed`).
constexpr const char* kFinishTheEdit = "finish the edit first -- commit it or cancel it";

/// A COMMIT ASKED FOR WHILE ONE IS UNANSWERED (`act`): not sent, and the draft and its edits stand.
/// It belongs to the unanswered commit, whose answer retires or replaces it (`answered_commit`).
constexpr const char* kCommitNotSent = "commit not sent -- an earlier commit is still unanswered";

/// ENDING A DRAFT WITH NO COMMIT UNANSWERED (`end_draft`). What the draft held is gone, and a write
/// an earlier commit made is not: neither sentence says whether anything was written.
constexpr const char* kCancelled = "edit cancelled -- unwritten changes discarded";
constexpr const char* kAbandoned =
    "edit abandoned -- the property it was on is no longer shown; unwritten changes discarded";

/// ...AND WITH ONE UNANSWERED, which a draft's end cannot take back.
constexpr const char* kCancelledSent =
    "commit already sent -- the draft is closed, and it may still be written";
constexpr const char* kAbandonedSent =
    "commit already sent -- its property is no longer shown, and it may still be written";

/// A COMMIT'S ACCOUNT OVER A DRAFT THAT DOES NOT HOLD EXACTLY WHAT IT SENT -- its own draft typed
/// into since, or a newer one -- and over no draft at all (`answered_commit`). A refusal is
/// followed by the document's own words.
constexpr const char* kEarlierWritten = "earlier commit written -- later edits not sent";
constexpr const char* kEarlierRefused = "earlier commit refused -- ";
constexpr const char* kLateWritten = "commit written -- it was sent before the draft closed";
constexpr const char* kLateRefused = "commit refused -- ";

constexpr bool available(Availability a) noexcept { return a == Availability::kAvailable; }

constexpr Availability action_availability(std::size_t which, bool editing,
                                           bool has_target) noexcept {
    if (editing) {
        return Availability::kDraftLive; // both controls; the reason is about the MAKER
    }
    if (which == kActionDelete && !has_target) {
        return Availability::kNoTarget;
    }
    return Availability::kAvailable;
}

constexpr const char* action_label(std::size_t which) noexcept {
    return which == kActionCreate ? "Create" : "Delete";
}

/// ONE CONTROL AS PROSE -- and the availability is said in CHARACTERS, not in colour, so a
/// medium with no ground to tint reads it too.
std::string action_row_text(std::size_t which, bool pressable, std::int64_t columns) {
    const std::string open = pressable ? "[ " : "( ";
    const std::string close = pressable ? " ]" : " )";
    return fit(open + action_label(which) + close, columns);
}

// =============================================================================
// The weave
// =============================================================================

class InfoPaneWeave
    : public loom::WeaveBase<
          InfoPaneWeave, pane::InfoPaneState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed, PaneKey,
                       PaneTextInput, PaneActionRequested, DocumentShown, DocumentActed,
                       surface::ClipboardCopy, surface::ClipboardText>,
          loom::Emit<PaneOffered, PaneActions, PaneContent, DocumentActRequested,
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

    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != pane::kInfoPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        granted_ = true;
        say(mail);
    }

    /// WHAT THE OBJECT DOCUMENT LOOKS LIKE, SAID BY THE HOST. Replaced WHOLE, never merged.
    void on(const DocumentShown& said, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return; // a stranger's opinion about a document is not a reading of one
        }
        known_ = said;
        heard_ = true;
        // ⚠ A DRAFT WHOSE PROPERTY IS NO LONGER SHOWN IS ABANDONED, and it is the one thing this
        // pane drops without being asked -- so it says so. The rows are the SELECTION's: a
        // selection moved by any gesture has left the field the maker was typing in, and every
        // object has `Name` on the same row, so the subject is the object AND the label at the
        // row. Carrying the draft onto whatever took its place would write the maker's text into
        // a different property.
        if (draft_.open && !shows_draft_subject()) {
            end_draft(kAbandoned, kAbandonedSent);
            declare(mail);
        }
        clamp_cursor();
        say(mail);
    }

    /// WHAT THE DOCUMENT MADE OF AN ACT THIS PANE IS WAITING ON, read against that act. The
    /// correlation says WHICH request an answer is about; the record it matches says what the
    /// request was, and a commit's draft incarnation and sent text say whether the draft open now
    /// is the one that sent it and holds nothing it did not send.
    ///
    /// An accepted act says nothing here -- the host says it on the band, as it always did; a
    /// picture it changed arrives as `DocumentShown`, and the notice the ask spent was already
    /// said away where the maker acted (`on(PanePressed)`, `on(PaneActionRequested)`). A refusal
    /// is the document's own words.
    void on(const DocumentActed& said, loom::Mail& mail) {
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
        // A SELECT, A CREATE OR A DELETE CLOSES NO DRAFT: none of them is a draft's answer, and
        // one asked before a draft opened may be answered while it is open.
        if (!said.accepted) {
            notice_ = said.refusal;
            say(mail);
        }
    }

    /// A PRESS NAMES A ROW OF THIS PANE'S ROOM, and this pane knows which list that row is
    /// in because it composed them. An object row selects; a property row moves the cursor;
    /// a control row spends it.
    void on(const PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != pane::kInfoPane) {
            return;
        }
        const Placed at = placed(press.row);
        if (at.what == Placed::kNothing) {
            return; // a heading, a marker, a blank row, or space below the last control
        }
        // THE MAKER HAS ACTED, SO THE LAST ACT'S ANSWER IS SPENT -- and spent means gone from the
        // rows Workshop holds (`agents/panes.md`). A select and a control's act are asks whose
        // answer is still on its way, and an accepted select of the object already selected
        // brings no new picture at all, so when a notice stood and the act published nothing the
        // rows are said here, once, without it.
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
    /// WHAT ONE DECLARED ACTION DOES, BY ID -- with the notice already spent
    /// (`on(PaneActionRequested)`), and only for an id `answers` admitted in this mode.
    void act(const PaneActionRequested& asked, loom::Mail& mail) {
        if (draft_.open) {
            if (asked.id == pane::kActionCommit) {
                if (committing_.awaiting) {
                    // ONE COMMIT OUTSTANDING AT A TIME. A second would hide the first's answer, or
                    // be answered first; so it is not sent, aloud, and nothing else is touched: the
                    // draft, its edits and the first commit's record stand, and Return sends the
                    // edits once the first is answered. No retry is queued.
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
        if (asked.id == pane::kActionUp) {
            if (state_.cursor > 0) {
                --state_.cursor;
            }
        } else if (asked.id == pane::kActionDown) {
            if (state_.cursor + 1 < static_cast<std::int64_t>(known_.properties.size())) {
                ++state_.cursor;
            }
        } else if (asked.id == pane::kActionEdit) {
            begin_draft(mail);
            return;
        } else {
            return;
        }
        say(mail);
    }

    // ---- Offering and declaring ---------------------------------------------------------

    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kInfoPaneRole)
            .send_to_role(kWorkshopRole,
                          PaneOffered{pane::kInfoPane, pane::kInfoPaneName,
                                      pane::kInfoPaneSummary});
        declare(mail);
    }

    /// WHAT THIS PANE ANSWERS TO RIGHT NOW -- re-declared whenever the mode changes.
    ///
    /// A PANE IS ONE KEYBOARD CONTEXT, AND A MODE IS NOT A SECOND ONE (WL-FILES-16). The
    /// built-in's draft lived in a Workshop context of its own (`KeyContext::kDraft`) and
    /// could bind Return and Escape there without touching command mode; a pane's rows are
    /// joined into ONE map under its runtime handle. So while a maker is typing a value this
    /// pane declares two rows and no more, and every other key reaches it as an ordinary
    /// `PaneKey` for the line to consume -- which is what lets Backspace delete a character
    /// rather than meaning `info.down`.
    void declare(loom::Mail& mail) {
        PaneActions actions;
        actions.pane = pane::kInfoPane;
        actions.rows = action_rows();
        (void)mail.as_role(pane::kInfoPaneRole).send_to_role(kWorkshopRole, actions);
    }

    /// THE ROWS OF THE MODE THIS PANE IS IN -- what `declare` tells Workshop, and what `answers`
    /// reads, so what the pane acts on and what it said it acts on are one list.
    std::vector<PaneActionRow> action_rows() const {
        std::vector<PaneActionRow> rows;
        const auto row = [&rows](const char* id, const char* label, std::int64_t sc,
                                 std::int64_t mods = input::mod::kNone) {
            rows.push_back(PaneActionRow{id, label, sc, mods});
        };
        if (draft_.open) {
            row(pane::kActionCommit, "commit", input::scan::kReturn);
            row(pane::kActionCancel, "cancel", input::scan::kEscape);
        } else {
            row(pane::kActionUp, "row up", input::scan::kUp);
            row(pane::kActionDown, "row down", input::scan::kDown);
            row(pane::kActionEdit, "edit", input::scan::kReturn);
        }
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

    // ---- The four asks -------------------------------------------------------------------

    /// A DOCUMENT ACT THIS PANE IS WAITING ON (`ask`).
    struct Asked {
        bool awaiting = false;
        std::uint64_t pending = 0; ///< the correlation: which request an answer names
    };

    /// ...AND A COMMIT, read against the draft that sent it and what it sent. A draft incarnation
    /// is not its contents: typing changes the text and keeps the epoch, which is why a paste may
    /// land in a draft that has moved on and why a write does not cover what was typed after
    /// Return. The subject is the draft's own (`Draft::object`, `Draft::label`), fixed for its life
    /// because a picture that moves it abandons the draft; nothing here says where the host wrote.
    struct SentCommit : Asked {
        std::uint64_t draft = 0; ///< `TextBox::draft_epoch` when it was sent: which draft
        std::string text;        ///< ...and what it sent: whether that draft holds anything more
        std::string promise;     ///< the sentence said about it while it was unanswered
    };

    /// ONE COMMIT OUTSTANDING, AND ONE OTHER ACT, THE NEWEST OF ITS KIND. `act` asks for a commit
    /// only while none is unanswered, so a commit's record is never replaced while it waits, and
    /// whether one is unanswered decides what ending a draft may say (`end_draft`). A select, a
    /// create or a delete replaces the record of its own kind -- an older one's answer names a
    /// correlation nothing waits on and is dropped unread, its sentence already spent by the act
    /// that asked again -- and cannot replace the commit's.
    void ask(DocumentActRequested request, loom::Mail& mail) {
        const std::uint64_t correlation = ++asked_;
        if (request.act == ws::kDocumentCommit) {
            committing_ = SentCommit{};
            committing_.awaiting = true;
            committing_.pending = correlation;
            committing_.draft = draft_.line.draft_epoch();
            committing_.text = request.text;
        } else {
            acting_ = Asked{true, correlation};
        }
        (void)mail.as_role(pane::kInfoPaneRole)
            .send_to_role(kWorkshopRole, std::move(request), correlation);
    }

    /// A COMMIT'S ANSWER -- about the draft that sent it and what it sent, and about nothing else.
    ///
    /// It is said where the notice row holds this commit's own sentence (`SentCommit::promise`),
    /// or is empty while the draft that sent it is open; a sentence a later act said stands.
    void answered_commit(const SentCommit& was, const DocumentActed& said, loom::Mail& mail) {
        const bool own = !was.promise.empty() && notice_ == was.promise;
        const bool same_draft = draft_.open && draft_.line.draft_epoch() == was.draft;
        const bool may_say = own || (same_draft && notice_.empty());
        if (same_draft && draft_.line.text() == was.text) {
            // THE DRAFT HOLDS EXACTLY WHAT IT SENT: accepted ends it, and a sentence about the
            // commit pending goes with it; a refusal stands beside it in the document's words.
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
        // ⚠ ANY OTHER DRAFT IS EDITS NO WRITE COVERS -- this one typed into after Return, or a
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

    void ask_select(std::int64_t identity, loom::Mail& mail) {
        DocumentActRequested request;
        request.act = ws::kDocumentSelect;
        request.identity = identity;
        ask(std::move(request), mail);
    }

    void ask_commit(loom::Mail& mail) {
        DocumentActRequested request;
        request.act = ws::kDocumentCommit;
        request.row = draft_.row;
        request.text = draft_.line.text();
        ask(std::move(request), mail);
    }

    void press_action(std::size_t which, loom::Mail& mail) {
        const Availability can = action_availability(which, draft_.open, has_target());
        if (can == Availability::kDraftLive) {
            // THE APPLICATION'S OWN REFUSAL, MADE BEFORE THE OPERATION. A live draft is
            // unfinished work this act would destroy, and the pane is the party that knows
            // (WL-CTRL-03).
            notice_ = kFinishTheEdit;
            say(mail);
            return;
        }
        // NO TARGET GOES THROUGH AND THE DOCUMENT REFUSES, which is the other half of the
        // two-reasons-two-owners law: whose refusal it is decides where it is made.
        DocumentActRequested request;
        request.act = which == kActionCreate ? ws::kDocumentCreate : ws::kDocumentDelete;
        ask(std::move(request), mail);
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
        if (state_.cursor < 0 ||
            static_cast<std::size_t>(state_.cursor) >= known_.properties.size()) {
            return;
        }
        const ShownProperty& row = known_.properties[static_cast<std::size_t>(state_.cursor)];
        if (!row.editable) {
            notice_ = row.label + " is not authored -- it is what the workspace makes of the "
                                  "authored value";
            say(mail);
            return;
        }
        draft_.open = true;
        draft_.row = state_.cursor;
        draft_.label = row.label;
        draft_.object = known_.selected;
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
        draft_.object = 0;
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

    /// DOES THE PICTURE STILL SHOW THE PROPERTY THE DRAFT IS TYPED INTO -- the same object, with
    /// the same label on the draft's row. Another object's row of the same index and label is a
    /// different property.
    bool shows_draft_subject() const {
        return known_.selected == draft_.object &&
               draft_.row < static_cast<std::int64_t>(known_.properties.size()) &&
               known_.properties[static_cast<std::size_t>(draft_.row)].label == draft_.label;
    }

    bool has_target() const { return known_.selected != 0; }

    void clamp_cursor() {
        const std::int64_t total = static_cast<std::int64_t>(known_.properties.size());
        if (state_.cursor >= total) {
            state_.cursor = total == 0 ? 0 : total - 1;
        }
        if (state_.cursor < 0) {
            state_.cursor = 0;
        }
    }

    // ---- Where a press lands -------------------------------------------------------------

    /// WHAT A ROW OF THIS PANE'S ROOM IS. Composed once by `say` and inverted here, so a
    /// press cannot land where a row is not -- the one-geometry rule, on the pane's side of
    /// the seam now that there is no shared painter to ask.
    struct Placed {
        enum What { kNothing, kObject, kProperty, kAction };
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
        if (at.what == Placed::kObject) {
            if (draft_.open) {
                // A SELECT CHANGES THE ROWS A DRAFT IS TYPED INTO, the one object already
                // selected included, so a live draft holds it back as it holds back the controls:
                // refused here, before anything is asked, and the draft is exactly what it was.
                notice_ = kFinishTheEdit;
                say(mail);
                return;
            }
            ask_select(known_.objects[at.index].identity, mail);
        } else if (at.what == Placed::kProperty) {
            state_.cursor = static_cast<std::int64_t>(at.index);
            say(mail);
        } else {
            press_action(at.index, mail);
        }
    }

    // ---- Saying what the pane shows -------------------------------------------------------

    void say(loom::Mail& mail) {
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
        // published place is where it lands in `out` plus `lead()`. Counting the notice the other
        // way sent a press two rows below the one it named whenever a notice stood.
        const auto mark = [&out, this](Placed::What what, std::size_t index) {
            composed_.push_back(Row{static_cast<std::int64_t>(out.size()) + lead(), what, index});
        };
        if (!heard_) {
            push("OBJECTS (waiting)", surface::role::kMuted);
            finish(out, mail);
            return;
        }
        // THE BUDGET, TAKEN BEFORE EITHER LIST IS OFFERED ANYTHING: two headings, the two
        // controls, and the notice row when there is one. `paint_info`'s own order, and the
        // reason is the same -- a bound that grows when it is exceeded is not a bound.
        const std::int64_t reserved = 2 + static_cast<std::int64_t>(kActionCount) +
                                      (notice_.empty() ? 0 : 1);
        const std::int64_t budget = rows_ - reserved;
        push("OBJECTS", surface::role::kAccent);
        if (budget <= 0) {
            finish(out, mail); // room for the heading and nothing else: no invented room
            return;
        }
        const BodyShare share =
            share_body_rows(static_cast<std::size_t>(budget), known_.objects.size(),
                            known_.properties.size());
        say_objects(share.objects, push, mark);
        while (out.size() < 1 + share.objects) {
            push(std::string(), surface::role::kFill);
        }
        // A SECTION BEGINS HERE, AND THE GROUND IS WHAT SAYS SO. Accent ink alone was not
        // enough: the row above `PROPERTIES` is the SELECTED object, which is accent too.
        push("PROPERTIES", surface::role::kAccent, surface::role::kMuted);
        say_properties(share.properties, push, mark);
        while (out.size() < 2 + share.objects + share.properties) {
            push(std::string(), surface::role::kFill);
        }
        for (std::size_t which = 0; which < kActionCount; ++which) {
            const bool pressable =
                available(action_availability(which, draft_.open, has_target()));
            mark(Placed::kAction, which);
            push(action_row_text(which, pressable, columns_),
                 pressable ? surface::role::kFill : surface::role::kMuted,
                 pressable ? surface::role::kMuted : surface::role::kNone);
        }
        finish(out, mail);
    }

    /// HOW MANY ROWS `finish` PUTS IN FRONT OF THE BODY: the notice, when there is one and a room
    /// to hold it beside the body. The press inverse adds the same number, so a notice cannot
    /// move a press off its row.
    std::int64_t lead() const { return !notice_.empty() && rows_ > 1 ? 1 : 0; }

    template <class Push, class Mark>
    void say_objects(std::size_t share, Push&& push, Mark&& mark) {
        if (known_.objects.empty()) {
            // AN EMPTY DOCUMENT SAYS IT IS EMPTY, and says what to do next -- a maker can
            // reach this state with their own hand, and a panel that merely goes blank is
            // indistinguishable from a tool that has broken.
            //
            // ⚠ AND IT IS SAID WHATEVER THE SHARE IS, which is a repair rather than a
            // preference. `share_body_rows` gives an EMPTY list a share of zero -- it wants
            // nothing, so it is offered nothing -- and this row was behind `share > 0`, so
            // the one document state the sentence exists for was the one state it never
            // appeared in: a maker who deleted their last object saw a blank column. The
            // built-in said it unconditionally (`paint_info`, before this migration) and
            // padded afterwards, which is what this does. The room is still the wall:
            // `finish` truncates to the granted rows and cannot be talked past.
            (void)share;
            push("(none) -- n makes one", surface::role::kMuted);
            return;
        }
        std::size_t selected_at = 0;
        for (std::size_t i = 0; i < known_.objects.size(); ++i) {
            if (known_.objects[i].identity == known_.selected) {
                selected_at = i;
            }
        }
        const ListWindow win = list_window(known_.objects.size(), selected_at, share);
        if (win.before > 0) {
            push(omitted_text(win.before, "earlier"), surface::role::kMuted);
        }
        for (std::size_t n = 0; n < win.count; ++n) {
            const std::size_t i = win.first + n;
            const ShownObject& o = known_.objects[i];
            const bool chosen = o.identity == known_.selected;
            mark(Placed::kObject, i);
            push(std::string(chosen ? "> " : "  ") + "#" + std::to_string(o.identity) + " " +
                     o.name,
                 chosen ? surface::role::kAccent : surface::role::kFill);
        }
        if (win.after > 0) {
            push(omitted_text(win.after, "more"), surface::role::kMuted);
        }
    }

    template <class Push, class Mark>
    void say_properties(std::size_t share, Push&& push, Mark&& mark) {
        if (known_.properties.empty()) {
            (void)share; // said whatever the share is, for `say_objects`' reason
            push("(nothing selected)", surface::role::kMuted);
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
            const bool here = i == focus;
            const bool editing = draft_.open && i == static_cast<std::size_t>(draft_.row);
            std::int64_t role = surface::role::kFill;
            if (editing) {
                role = surface::role::kAlert; // a live draft is never quiet
            } else if (!p.editable) {
                role = surface::role::kMuted; // not the maker's to author
            }
            std::string text = std::string(here ? ">" : " ") +
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
            out.insert(out.begin(), surface::SurfaceTextRow{drawable(fit(notice_, columns_)),
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
        std::string label;       ///< what the row was called when the draft opened
        std::int64_t object = 0; ///< ...and whose property: the selection that picture named
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

    zengine::ActivationCursor activation_;

    /// THE HOST'S LAST READING OF ITS OWN DOCUMENT. Replaced whole; owned by nobody here;
    /// deliberately NOT in the state shape.
    DocumentShown known_;
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
    Asked acting_;         ///< a select, a create or a delete
    SentCommit committing_; ///< a commit

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(InfoPaneWeave)
