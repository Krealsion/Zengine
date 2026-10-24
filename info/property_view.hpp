// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_INFO_PROPERTY_VIEW_HPP
#define ZENGINE_INFO_PROPERTY_VIEW_HPP

// Info's pane-property view, the default pane `info` while it holds no value: the properties of
// the one pane an office named as its subject (`InspectPaneRequested`, the Pane Manager's
// `inspect in Info`), as the host says them (`PaneSubjectShown`), and a draft of one of them the
// host writes when it is committed (`PaneCommitRequested`). It touches no desk: it asks.
// Reference: info/docs/zengine.info.md.
// Workshop law: agents/workshop/info-body.md

// The view composes its rows and their press map once, in the room it was granted, and after
// each act says what the weave owes Workshop (`Due`): its action rows again, its rows again. The
// weave sends both, through the view's canvas or as prose, and keeps the clipboard's custody.

#include "vocabulary.hpp"
#include "value_watch.hpp"

#include "workshop/inspection_seam_vocabulary.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_parts.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "component/text_box.hpp"
#include "graphics/role.hpp"
#include "input/vocabulary.hpp"
#include "graphics/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::info_pane {

/// WHO THIS PANE IS TALKING TO -- the host's office, spelled as a literal exactly as the
/// other pane weaves spell it.
inline constexpr const char* kWorkshopRole = "zengine.workshop";

class PropertyView {
public:
    /// WHAT THE WEAVE OWES WORKSHOP AFTER AN ACT OF THIS VIEW: its action rows again, when what
    /// Return and the arrows mean changed, and then its rows again, when anything it shows did.
    struct Due {
        bool declare = false;
        bool say = false;
    };
    Due take_due() { return std::exchange(due_, Due{}); }

    /// The property row the weaver is on, as `InfoPaneState` carries it across a reload.
    std::int64_t cursor() const noexcept { return cursor_; }
    void revive(std::int64_t cursor) { cursor_ = cursor; }

    bool drafting() const noexcept { return draft_.open; }
    bool committing() const noexcept { return committing_.awaiting; }
    /// An ask of this view's still unanswered: a commit, or an inspect.
    bool waiting() const noexcept { return committing_.awaiting || acting_.awaiting; }
    /// THE DRAFT INCARNATION (`component::TextBox::draft_epoch`) a paste is asked for.
    std::uint64_t draft_epoch() const { return draft_.line.draft_epoch(); }

    /// The rows and columns the view composes for, as the weave reads them from its room.
    void room(std::int64_t rows, std::int64_t columns) {
        rows_ = rows;
        columns_ = columns;
        granted_ = true;
    }
    bool granted() const noexcept { return granted_ && rows_ > 0 && columns_ > 0; }

    /// A SENTENCE FOR THE WEAVER that an act this view does not own left: said above the rows the
    /// next time they are said, and spent by the next act.
    void tell(std::string sentence) { notice_ = std::move(sentence); }

    /// THE DEMO AND TEST RESET: the draft goes and the notice with it. Asked only while no ask of
    /// this view's waits (`waiting`).
    void reset() {
        draft_ = Draft{};
        notice_.clear();
    }

    // ---- The host's readings and answers ------------------------------------------------

    /// WHAT THE SUBJECT LOOKS LIKE, SAID BY THE HOST, AND WHAT ITS ROWS ADDRESS. Replaced WHOLE,
    /// never merged -- published when it changes, or answered to this image's arrival.
    void shown(const workshop::PaneSubjectShown& said) {
        known_ = said;
        // (!) A DRAFT WHOSE ROWS ARE NO LONGER SHOWN IS ABANDONED, and it is the one thing this
        // pane drops without being asked -- so it says so. The host names what the rows address:
        // another pane, another desk put live, or another layout of rows is another name, even
        // where `Width` sits on the same row with the same value. Carrying the draft onto
        // whatever took its place would ask the owner to write the weaver's text into a
        // different property, and it would refuse.
        if (draft_.open && !shows_draft_subject()) {
            end_draft(kAbandoned, kAbandonedSent);
            due_.declare = true;
        }
        clamp_cursor();
        due_.say = true;
    }

    /// What the owner made of an ask this view is waiting on, read against that ask: the
    /// correlation says which request, the record says what it was, and a commit's draft
    /// incarnation and sent text say whether the open draft sent it. An accepted ask says
    /// nothing here (the host says a write, and a changed picture arrives); a refusal is the
    /// owner's own words. The weave has already judged that the mail answers an ask.
    void acted(const workshop::PaneSubjectActed& said, loom::Mail& mail) {
        if (committing_.awaiting && mail.correlation() == committing_.pending) {
            const SentCommit was = std::move(committing_);
            committing_ = SentCommit{};
            answered_commit(was, said);
            return;
        }
        if (!acting_.awaiting || mail.correlation() != acting_.pending) {
            return;
        }
        acting_ = Asked{};
        settle_relay(said, mail);
        // AN INSPECT CLOSES NO DRAFT: one asked before a draft opened may be answered while it is
        // open, and the picture that follows it is what decides whether the draft still stands.
        if (!said.accepted) {
            notice_ = said.refusal;
            due_.say = true;
        }
    }

    /// AN OFFICE ASKS THIS PANE TO INSPECT A PANE -- the Pane Manager's `inspect in Info` row --
    /// and the ask is this pane's own inspect (`ask_inspect`): held back while a draft is live,
    /// since another subject would take the draft's rows away, and the host's answer passed back
    /// whole. It is the weaver's act on this view, so it spends the notice before it asks. The
    /// weave has already judged that an office asked.
    void inspect_asked(const workshop::InspectPaneRequested& asked, ViewContext& c) {
        if (draft_.open) {
            notice_ = kFinishTheEdit;
            due_.say = true;
            (void)c.mail.answer(workshop::PaneSubjectActed{false, kFinishTheEdit});
            return;
        }
        spend_notice();
        loom::DeferredAnswer owed = c.mail.defer_answer();
        ask_inspect(asked.office, asked.pane, c);
        if (acting_.awaiting) {
            relay_ = std::move(owed);
        } else {
            (void)loom::answer_deferred(owed, c.mail, workshop::PaneSubjectActed{false, notice_});
        }
    }

    /// Loom's word that an ask of this view's never reached the door's handler
    /// (`zen.DispatchRefused`; WL-INFO-13): an attestation, not an answer. The exact queued
    /// attempt, its correlation and what it asked are matched against the one record that can
    /// still be waiting (`refused_ask`); an ask received and never answered stays awaited. Says
    /// whether the notice named one of this view's asks; the weave has judged its provenance.
    bool refused(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (refused_ask(committing_, refused, mail, workshop::PaneCommitRequested::zen_name,
                        workshop::PaneCommitRequested::zen_version)) {
            const SentCommit was = std::move(committing_);
            committing_ = SentCommit{};
            undelivered_commit(was, refused);
            return true;
        }
        if (refused_ask(acting_, refused, mail, workshop::InspectPaneRequested::zen_name,
                        workshop::InspectPaneRequested::zen_version)) {
            const std::string act = acting_.act;
            acting_ = Asked{};
            notice_ = act + kActUndelivered + " (" + refused.reason + ")";
            settle_relay(workshop::PaneSubjectActed{false, notice_}, mail);
            due_.say = true;
            return true;
        }
        return false;
    }

    // ---- The weaver's acts ------------------------------------------------------------------

    /// A PRESS NAMES A ROW OF THE VIEW'S ROOM, and this view knows what that row is because it
    /// composed it: a property row moves the cursor there, read against the rows last composed.
    void press(std::int64_t row) {
        const Placed at = placed(row);
        if (at.what == Placed::kNothing) {
            return; // a heading, a marker, a blank row
        }
        spend_notice();
        press_placed(at);
    }

    /// ONLY THE DRAFT READS RAW KEYS. Everything else this view does arrives as a resolved id; a
    /// component's editing gestures are the component's, not the pane's commands. Says whether
    /// the line took the key: one that means nothing to it is no act, and the notice stands,
    /// unsaid.
    bool key(std::int64_t scancode, std::int64_t modifiers, component::Clipboard& clip) {
        if (!draft_.open || !draft_.line.consume(scancode, modifiers, clip)) {
            return false;
        }
        notice_.clear();
        due_.say = true;
        return true;
    }

    void type(const std::string& text) {
        if (!draft_.open || text.empty() || !workshop::pane_text::admissible(text)) {
            return;
        }
        notice_.clear();
        draft_.line.type(text);
        due_.say = true;
    }

    /// CLIPBOARD TEXT ASKED FOR BY THE DRAFT OF `epoch`: the draft that asked is over when its
    /// epoch is, and a later one did not ask, so it gets nothing.
    void pasted(const std::string& text, std::uint64_t epoch) {
        if (!draft_.open || draft_.line.draft_epoch() != epoch) {
            return;
        }
        if (text.empty() || !workshop::pane_text::admissible(text)) {
            return;
        }
        draft_.line.type(text);
        due_.say = true;
    }

    /// WHAT ONE DECLARED ACTION DOES, BY ID -- only for an id `answers` admitted in this mode, and
    /// with the notice spent first: the weaver has acted, so the last act's answer is spent, in
    /// the rows Workshop holds too -- a commit whose answer is on its way, or an edit with nothing
    /// to edit, says nothing of its own.
    void act(const std::string& id, ViewContext& c) {
        spend_notice();
        if (draft_.open) {
            if (id == kActionCommit) {
                if (committing_.awaiting) {
                    // ONE COMMIT OUTSTANDING AT A TIME. A second would hide the first's answer, or
                    // be answered first; so it is not sent, aloud, and nothing else is touched: the
                    // draft, its edits and the first commit's record stand, and Return sends the
                    // edits once the first is answered -- or once Loom says it never arrived
                    // (`refused`). No retry is queued, and delivered silence still waits.
                    notice_ = kCommitNotSent;
                    committing_.promise = notice_;
                    due_.say = true;
                    return;
                }
                ask_commit(c);
            } else if (id == kActionCancel) {
                // ESCAPE ALWAYS ENDS THE DRAFT, AND SAYS WHETHER THAT WAS ALL IT ENDED.
                end_draft(kCancelled, kCancelledSent);
                due_.declare = true;
                due_.say = true;
            }
            return;
        }
        if (id == kActionEdit) {
            begin_draft();
            return;
        }
        if (id == kActionUp || id == kActionDown) {
            step_property(id == kActionUp ? -1 : +1);
            due_.say = true;
        }
    }

    /// THE ROWS OF THE MODE THIS VIEW IS IN -- what the weave declares and `answers` reads, so what
    /// the view acts on and what it said it acts on are one list. A PANE IS ONE KEYBOARD CONTEXT,
    /// AND A MODE IS NOT A SECOND ONE (WL-FILES-16): while a weaver types a value this view
    /// declares two rows and no more, and every other key reaches it as an ordinary `PaneKey` for
    /// the line to consume, so Backspace deletes a character rather than meaning anything here.
    std::vector<workshop::v3::PaneActionRow> action_rows() const {
        std::vector<workshop::v3::PaneActionRow> rows;
        const auto row = [&rows](const char* id, const char* label, std::int64_t sc,
                                 std::vector<workshop::ActionCause> causes = {}) {
            rows.push_back(workshop::v3::PaneActionRow{id, label, sc, input::mod::kNone, {},
                                                       std::move(causes)});
        };
        if (draft_.open) {
            row(kActionCommit, "commit", input::scan::kReturn,
                {workshop::cause_of<workshop::PaneCommitRequested>()});
            row(kActionCancel, "cancel", input::scan::kEscape);
        } else {
            row(kActionUp, "row up", input::scan::kUp);
            row(kActionDown, "row down", input::scan::kDown);
            row(kActionEdit, "edit", input::scan::kReturn);
        }
        return rows;
    }

    bool answers(const std::string& id) const {
        for (const workshop::v3::PaneActionRow& row : action_rows()) {
            if (row.id == id) {
                return true;
            }
        }
        return false;
    }

    // ---- Saying what the view shows -------------------------------------------------------

    /// THE ROWS THIS VIEW SHOWS, composed once into the room it was granted, with their press map
    /// (`placed`) and the draft's caret (`caret`): the front sentence, the one heading, and the
    /// subject's properties under it.
    std::vector<workshop::TextRow> compose() {
        std::vector<workshop::TextRow> out;
        composed_.clear();
        caret_ = {};
        const auto push = [&out, this](std::string text, std::int64_t role,
                                       std::int64_t ground = graphics::role::kNone) {
            out.push_back(workshop::TextRow{
                workshop::pane_text::drawable(workshop::pane_text::fit(std::move(text), columns_)),
                role, ground});
        };
        // THE BODY IS COMPOSED WITHOUT THE NOTICE, and `finish` puts it in front, so a row's
        // published place is where it lands in `out` plus `lead()`.
        const auto mark = [&out, this](Placed::What what, std::size_t index) {
            composed_.push_back(Row{static_cast<std::int64_t>(out.size()) + lead(), what, index});
        };
        // THE BUDGET, TAKEN BEFORE THE LIST IS OFFERED ANYTHING: the heading, and the front row
        // when there is one -- a bound that grows when it is exceeded is not a bound.
        const std::int64_t reserved = 1 + (front_sentence().empty() ? 0 : 1);
        const std::int64_t budget = rows_ - reserved;
        // THE LIST BEGINS HERE, AND THE GROUND IS WHAT SAYS SO: the front sentence above it is
        // alert ink, and the heading names whose properties follow.
        push(known_.subject == 0 ? std::string("PROPERTIES") : "PANE " + known_.name,
             graphics::role::kAccent, graphics::role::kMuted);
        if (budget > 0) {
            say_properties(static_cast<std::size_t>(budget), push, mark);
        }
        return finish(std::move(out)); // room for the heading and nothing else: no invented room
    }

    /// WHAT INFO CALLS THE ROWS IT SENDS, of the `said` it sends: a property by its label,
    /// `property:<label>`. A row it names nothing is not listed: a row entire lies over no other
    /// part.
    std::vector<workshop::RowPart> named_rows(std::int64_t said) const {
        workshop::PartNames<workshop::RowPart> names;
        for (const Row& r : composed_) {
            if (r.row >= said || r.what != Placed::kProperty ||
                r.index >= known_.properties.size()) {
                continue;
            }
            (void)names.add(workshop::RowPart{"property:" + known_.properties[r.index].label,
                                                  r.row, 0, columns_});
        }
        return names.take();
    }

    /// The draft's caret and selection in the rows last composed, or none.
    const workshop::RowsCaret& caret() const noexcept { return caret_; }

private:
    // ---- The measurements and the sentences ----------------------------------------------

    static constexpr std::int64_t kPropertyMarkCols = 1;  ///< the `>` that marks the cursor's row
    static constexpr std::int64_t kPropertyLabelCols = 9; ///< the label column a value is measured after

    /// THE PANE'S OWN REFUSAL WHILE A DRAFT IS LIVE -- for every act the draft holds back: an
    /// office naming another subject.
    static constexpr const char* kFinishTheEdit = "finish the edit first -- commit it or cancel it";

    /// A COMMIT ASKED FOR WHILE ONE IS UNANSWERED (`act`): not sent, and the draft and its edits
    /// stand. It belongs to the unanswered commit, whose answer retires or replaces it
    /// (`answered_commit`).
    static constexpr const char* kCommitNotSent =
        "commit not sent -- an earlier commit is still unanswered";

    /// ENDING A DRAFT WITH NO COMMIT UNANSWERED (`end_draft`). What the draft held is gone, and a
    /// write an earlier commit made is not: neither sentence says whether anything was written. A
    /// draft is abandoned when the host's picture names other rows (`shows_draft_subject`) --
    /// another pane, another desk put live, or the rows' layout changing -- so the sentence names
    /// all three.
    static constexpr const char* kCancelled = "edit cancelled -- unwritten changes discarded";
    static constexpr const char* kAbandoned =
        "edit abandoned -- the inspected pane, its desk or its rows changed; unwritten changes "
        "discarded";

    /// ...AND WITH ONE UNANSWERED, which a draft's end cannot take back. A cancel moves no subject,
    /// so its commit may still be written; an abandonment promises nothing about the one it leaves.
    static constexpr const char* kCancelledSent =
        "commit already sent -- the draft is closed, and it may still be written";
    static constexpr const char* kAbandonedSent =
        "commit already sent -- the draft ended: the inspected pane, its desk or its rows changed";

    /// A COMMIT'S ACCOUNT OVER A DRAFT THAT DOES NOT HOLD EXACTLY WHAT IT SENT -- its own draft
    /// typed into since, or a newer one -- and over no draft at all (`answered_commit`). A
    /// refusal is followed by the owner's own words.
    static constexpr const char* kEarlierWritten = "earlier commit written -- later edits not sent";
    static constexpr const char* kEarlierRefused = "earlier commit refused -- ";
    static constexpr const char* kLateWritten = "commit written -- it was sent before the draft closed";
    static constexpr const char* kLateRefused = "commit refused -- ";

    /// AN ASK THAT NEVER REACHED THE DOOR (WL-INFO-13): nothing queued at all (the send's ticket
    /// is not valid), or queued and refused by Loom before the door's handler ran
    /// (`zen.DispatchRefused`, whose reason follows in parentheses). Either way nothing was
    /// written or changed, the record is released, and the draft and its text stand for the next
    /// Return.
    static constexpr const char* kCommitNotQueued = "commit not submitted -- nothing was queued or written";
    static constexpr const char* kCommitUndelivered = "commit not delivered -- nothing was written";
    static constexpr const char* kEarlierUndelivered = "earlier commit not delivered -- nothing was written";
    static constexpr const char* kActNotQueued = " not submitted -- nothing was queued";
    static constexpr const char* kActUndelivered = " not delivered -- nothing changed";
    static constexpr const char* kInspectReplaced = "a newer inspect took its place -- nothing changed";

    /// WHAT AN INSPECT IS CALLED in the two sentences above.
    static constexpr const char* kInspectAct = "inspect";

    /// NOTHING INSPECTED YET, said where the properties would be and when Return asks for a
    /// draft: the subject is named from the Pane Manager.
    static constexpr const char* kNoSubjectRow = "(no subject -- 'inspect in Info' in the Pane Manager)";
    static constexpr const char* kNoSubject =
        "nothing is inspected yet -- choose 'inspect in Info' in the Pane Manager";

    /// A row this pane will not open a draft on: the host's own sentence, said before asking.
    static std::string not_authored(const std::string& label) {
        return label + " is not authored -- it is what the screen makes of the authored value";
    }

    /// WHAT A WINDOW OVER A LIST LOOKS LIKE -- `screen.hpp`'s `ListWindow`, carried byte-for-byte
    /// from the host's presentation, since `screen.hpp` is not a header a loaded image may include.
    struct ListWindow {
        std::size_t first = 0;
        std::size_t count = 0;
        std::size_t before = 0;
        std::size_t after = 0;
    };

    /// THE THREE RULES, CARRIED (`screen_gestures.cpp`): a population that fits is shown whole,
    /// the focused row is always in the window, and every omission is counted on its own side
    /// and spends a row of the same budget.
    static ListWindow list_window(std::size_t total, std::size_t selected_at, std::size_t rows) {
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

    // ---- The cursor ---------------------------------------------------------------------------

    /// THE CURSOR MOVES OVER ROWS AND STEPS OVER SECTION HEADINGS, which hold no value.
    void step_property(std::int64_t by) {
        const std::int64_t n = static_cast<std::int64_t>(known_.properties.size());
        std::int64_t at = cursor_;
        while (true) {
            at += by;
            if (at < 0 || at >= n) {
                return; // at the edge: nothing moved
            }
            if (!known_.properties[static_cast<std::size_t>(at)].section) {
                cursor_ = at;
                return;
            }
        }
    }

    /// KEEP THE CURSOR ON A ROW THAT EXISTS AND HOLDS A VALUE.
    void clamp_cursor() {
        const std::int64_t total = static_cast<std::int64_t>(known_.properties.size());
        if (cursor_ >= total) {
            cursor_ = total == 0 ? 0 : total - 1;
        }
        if (cursor_ < 0) {
            cursor_ = 0;
        }
        if (total > 0 && known_.properties[static_cast<std::size_t>(cursor_)].section) {
            step_property(+1);
        }
    }

    // ---- The asks -----------------------------------------------------------------------

    /// AN ASK THIS VIEW IS WAITING ON (`ask_inspect`, `ask_commit`).
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

    /// The office's ask, answered with what became of the inspect it started: once, and only
    /// while one is held.
    void settle_relay(const workshop::PaneSubjectActed& said, loom::Mail& mail) {
        if (relay_.valid()) {
            (void)loom::answer_deferred(relay_, mail, said);
            relay_ = loom::DeferredAnswer{};
        }
    }

    /// ONE INSPECT OUTSTANDING, THE NEWEST: an older one's answer names a correlation nothing
    /// waits on and is dropped unread, its office told it was replaced; the commit's record is
    /// never replaced by it. (!) THE TICKET IS KEPT (WL-INFO-13): one not valid means nothing was
    /// queued and no answer or refusal can follow, so the record is released at once, aloud. A
    /// queued inspect changes nothing shown: the picture its answer brings says it.
    void ask_inspect(const std::string& office, const std::string& pane_key, ViewContext& c) {
        settle_relay(workshop::PaneSubjectActed{false, kInspectReplaced}, c.mail);
        const std::uint64_t correlation = ++c.asks;
        Asked asking;
        asking.awaiting = true;
        asking.pending = correlation;
        asking.act = kInspectAct;
        asking.attempt = c.mail.as_role(kInfoPaneRole)
                             .send_to_role(kWorkshopRole,
                                           workshop::InspectPaneRequested{office, pane_key},
                                           correlation);
        if (!asking.attempt.valid()) {
            acting_ = Asked{};
            notice_ = std::string(kInspectAct) + kActNotQueued;
            due_.say = true;
            return;
        }
        acting_ = std::move(asking);
    }

    /// DOES THIS NOTICE NAME THE ASK `record` IS WAITING ON -- the same queued attempt, under the
    /// same correlation, of the shape it sent, to the office it addressed? Every half, because a
    /// sequence alone is a number and the rest is what this view asked.
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
    void answered_commit(const SentCommit& was, const workshop::PaneSubjectActed& said) {
        const bool own = !was.promise.empty() && notice_ == was.promise;
        const bool same_draft = draft_.open && draft_.line.draft_epoch() == was.draft;
        const bool may_say = own || (same_draft && notice_.empty());
        if (same_draft && draft_.line.text() == was.text) {
            // THE DRAFT HOLDS EXACTLY WHAT IT SENT: accepted ends it, and a sentence about the
            // commit pending goes with it; a refusal stands beside it in the owner's words.
            if (said.accepted) {
                close_draft();
                due_.declare = true;
                if (own) {
                    notice_.clear();
                }
                due_.say = true;
            } else if (may_say) {
                notice_ = said.refusal;
                due_.say = true;
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
        due_.say = true;
    }

    /// A COMMIT NAMES THE ROWS ITS DRAFT WAS OPENED ON, THE ROW AND THE TEXT, and keeps its ticket
    /// as an inspect does: nothing queued releases the record at once, and the draft and its text
    /// stand, so the next Return is a fresh attempt rather than a second commit.
    void ask_commit(ViewContext& c) {
        workshop::PaneCommitRequested request;
        request.subject = draft_.subject;
        request.row = draft_.row;
        request.text = draft_.line.text();
        const std::uint64_t correlation = ++c.asks;
        SentCommit sending;
        sending.awaiting = true;
        sending.pending = correlation;
        sending.act = "commit";
        sending.draft = draft_.line.draft_epoch();
        sending.text = request.text;
        sending.attempt = c.mail.as_role(kInfoPaneRole)
                              .send_to_role(kWorkshopRole, std::move(request), correlation);
        if (!sending.attempt.valid()) {
            committing_ = SentCommit{};
            notice_ = kCommitNotQueued;
            due_.say = true;
            return;
        }
        committing_ = std::move(sending);
    }

    /// A COMMIT LOOM REFUSED BEFORE THE DOOR RAN -- nothing was written, so it closes no draft: the
    /// one that sent it keeps its text and its history, and Return sends what it holds. The account
    /// is said where an answer's would be (`answered_commit`): over this commit's own sentence, or
    /// over an empty row while its own draft is open; a sentence a later act said stands.
    void undelivered_commit(const SentCommit& was, const loom::DispatchRefused& refused) {
        const bool own = !was.promise.empty() && notice_ == was.promise;
        const bool same_draft = draft_.open && draft_.line.draft_epoch() == was.draft;
        if (!own && !(same_draft && notice_.empty())) {
            return;
        }
        const std::string why = " (" + refused.reason + ")";
        notice_ = (draft_.open && !same_draft ? std::string(kEarlierUndelivered)
                                              : std::string(kCommitUndelivered)) +
                  why;
        due_.say = true;
    }

    // ---- The draft ----------------------------------------------------------------------

    void begin_draft() {
        if (known_.subject == 0) {
            notice_ = kNoSubject;
            due_.say = true;
            return;
        }
        if (cursor_ < 0 || static_cast<std::size_t>(cursor_) >= known_.properties.size()) {
            return;
        }
        const workshop::ShownProperty& row = known_.properties[static_cast<std::size_t>(cursor_)];
        if (!row.editable) {
            notice_ = not_authored(row.label);
            due_.say = true;
            return;
        }
        draft_.open = true;
        draft_.row = cursor_;
        draft_.label = row.label;
        draft_.subject = known_.subject;
        draft_.line.set(row.value, row.value.size()); // the caret at the end, as it was
        due_.declare = true;
        due_.say = true;
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

    /// End the live draft and say what ending it did: `ended` when no commit is unanswered,
    /// `sent` while one is. Closing a draft is always this view's; a write already asked for is
    /// not, so `sent` promises neither outcome and the commit's record may replace it
    /// (`answered_commit`). `ended` says only that what was never written is gone.
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

    // ---- Where a press lands -------------------------------------------------------------

    /// WHAT A ROW OF THIS VIEW'S ROOM IS. Composed once by `compose` and inverted here, so a
    /// press cannot land where a row is not -- the one-geometry rule, on the pane's side of
    /// the seam now that there is no shared painter to ask.
    struct Placed {
        enum What { kNothing, kProperty };
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

    /// THE WEAVER HAS ACTED, SO THE LAST ACT'S ANSWER IS SPENT -- and spent means gone from the
    /// rows Workshop holds (`agents/panes.md`): an act that changes nothing shown, or an inspect
    /// whose answer is still on its way and may bring no new picture, still says the rows once
    /// without the notice.
    void spend_notice() {
        if (!notice_.empty()) {
            notice_.clear();
            due_.say = true;
        }
    }

    /// WHAT A PRESS ON A PLACED ROW DOES -- with the notice already spent (`press`): the cursor
    /// goes to the property pressed, or, while a draft is live, waits there for it to end.
    void press_placed(const Placed& at) {
        // A COMPOSITION OLDER THAN THE READING: a list that shrank since the rows were said (a
        // room of zero says nothing and so re-composes nothing). The press names no row now.
        if (at.what != Placed::kProperty || at.index >= known_.properties.size()) {
            return;
        }
        if (known_.properties[at.index].section) {
            return;
        }
        cursor_ = static_cast<std::int64_t>(at.index);
        due_.say = true;
    }

    // ---- Composing ----------------------------------------------------------------------

    /// THE SENTENCE IN FRONT OF THE BODY: the notice, while one stands.
    const std::string& front_sentence() const { return notice_; }

    /// HOW MANY ROWS `finish` PUTS IN FRONT OF THE BODY: the front sentence, when there is one and
    /// a room to hold it beside the body. The press inverse adds the same number, so a sentence
    /// cannot move a press off its row.
    std::int64_t lead() const { return !front_sentence().empty() && rows_ > 1 ? 1 : 0; }

    template <class Push, class Mark>
    void say_properties(std::size_t share, Push&& push, Mark&& mark) {
        if (known_.subject == 0) {
            (void)share; // said whatever the share is: an empty list is offered nothing
            push(kNoSubjectRow, graphics::role::kMuted);
            return;
        }
        const std::size_t focus =
            draft_.open ? static_cast<std::size_t>(draft_.row)
                        : static_cast<std::size_t>(cursor_ < 0 ? 0 : cursor_);
        const ListWindow win = list_window(known_.properties.size(), focus, share);
        if (win.before > 0) {
            push(workshop::pane_text::omitted_text(win.before, "earlier"), graphics::role::kMuted);
        }
        const std::int64_t value_columns = columns_ - kPropertyMarkCols - kPropertyLabelCols;
        for (std::size_t n = 0; n < win.count; ++n) {
            const std::size_t i = win.first + n;
            const workshop::ShownProperty& p = known_.properties[i];
            if (p.section) {
                push(" " + p.label, graphics::role::kAccent);
                continue;
            }
            const bool here = i == focus;
            const bool editing = draft_.open && i == static_cast<std::size_t>(draft_.row);
            std::int64_t role = graphics::role::kFill;
            if (editing) {
                role = graphics::role::kAlert; // a live draft is never quiet
            } else if (!p.editable) {
                role = graphics::role::kMuted; // not the weaver's to author
            }
            std::string text =
                std::string(here || editing ? ">" : " ") +
                workshop::pane_text::pad(p.label, static_cast<std::size_t>(kPropertyLabelCols));
            if (editing) {
                // THE WINDOW FOLLOWS THE CARET, so a long value scrolls to where the weaver is
                // typing.
                draft_.line.keep_caret_visible(value_columns);
                text += draft_.line.visible(value_columns);
            } else {
                text += workshop::pane_text::fit(p.value, value_columns);
            }
            mark(Placed::kProperty, i);
            if (editing && value_columns > 0) {
                const std::int64_t at = kPropertyMarkCols + kPropertyLabelCols;
                caret_.row = composed_.back().row;
                caret_.column = at + static_cast<std::int64_t>(draft_.line.caret_column());
                if (const auto span = draft_.line.visible_selection(value_columns); span.present()) {
                    caret_.sel_begin_row = caret_.sel_end_row = caret_.row;
                    caret_.sel_begin_col = at + span.begin;
                    caret_.sel_end_col = at + span.end;
                }
            }
            push(std::move(text), role);
        }
        if (win.after > 0) {
            push(workshop::pane_text::omitted_text(win.after, "more"), graphics::role::kMuted);
        }
    }

    /// THE ROOM IS THE WALL: the front sentence goes in front of the body when there is a row for
    /// it, and whatever the room cannot hold is cut, whatever was composed.
    std::vector<workshop::TextRow> finish(std::vector<workshop::TextRow> out) const {
        if (lead() > 0) {
            if (static_cast<std::int64_t>(out.size()) > rows_ - 1) {
                out.resize(static_cast<std::size_t>(rows_ - 1));
            }
            out.insert(out.begin(), workshop::TextRow{
                                        workshop::pane_text::drawable(
                                            workshop::pane_text::fit(front_sentence(), columns_)),
                                        graphics::role::kAlert});
        }
        if (static_cast<std::int64_t>(out.size()) > rows_) {
            out.resize(static_cast<std::size_t>(rows_));
        }
        return out;
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

    /// THE HOST'S LAST READING OF THE SUBJECT, and the name of what its rows address. Replaced
    /// whole; owned by nobody here; deliberately NOT in the state shape.
    workshop::PaneSubjectShown known_;
    std::int64_t cursor_ = 0; ///< which property row the weaver is on: an index over a stable list

    Draft draft_;
    std::vector<Row> composed_;
    workshop::RowsCaret caret_; ///< the draft's caret and selection in the rows last composed, or none
    std::string notice_;
    Due due_;

    Asked acting_;               ///< an inspect
    loom::DeferredAnswer relay_; ///< the office that asked for `acting_`, owed its answer
    SentCommit committing_;      ///< a commit

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;
};

} // namespace zengine::info_pane

#endif // ZENGINE_INFO_PROPERTY_VIEW_HPP
