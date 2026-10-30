// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Terminal pane: a loadable weave that offers Workshop one pane -- the record of the
// terminal participant this host mounted, and the line a weaver composes messages on. The
// participant stays with the host (no message in its interface drives it), so what crosses is a
// picture the host derives (`TranscriptShown`), one act (`TerminalActRequested`) and one read
// (`TerminalCompletionRequested`): nothing here can speak as that participant, only ask it to.
// Workshop law: agents/workshop/terminal-pane.md

// The pane draws its rows, the line's caret and its selection in its own area of the window. A
// sweep is not a gesture here: a press places the caret, a second press in a word selects it, and
// shift with the caret keys sweeps by keyboard.

// This file is the weave: its offer, its settings, the record it is shown, its keys, text and
// presses, the value pickup, the clipboard, the act and the rows it declares. It declares every
// member; `view.hpp` defines the record's view and the rows composed from it, and
// `history_and_completion.hpp` the recall and the completion ask and cursor, both included after
// the class.

#include "terminal/vocabulary.hpp"

#include "workshop/pane_causes.hpp"
#include "workshop/pane_facets.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"

#include "activation/activation.hpp"
#include "component/text_box.hpp"
#include "component/row_map.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_settings.hpp"
#include "workshop/pane_settings_taken.hpp"
#include "workshop/settings_seam_vocabulary.hpp"
#include <zen/weave/dispatch_refusal.hpp>
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "workshop/pane_rows.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/role_request.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include "manuals/zengine.terminal.hpp" // this pane's manual, compiled in from its page

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace component = zengine::component;
namespace input = zengine::input;
namespace gfx = zengine::graphics;
namespace ws = zengine::workshop;
namespace pane = zengine::terminal_pane;

using ws::v4::PaneActionRow;
using ws::PaneEscapeUnspent;
using ws::PaneKey;
using ws::v2::PaneOffered;
using ws::PaneTextInput;
using ws::ShownCandidate;
using ws::ShownEntry;
using ws::TerminalActed;
using ws::TerminalActRequested;
using ws::TerminalCompletionOffered;
using ws::TerminalCompletionRequested;
using ws::TranscriptShown;
using zengine::workshop::pane_text::admissible;

constexpr const char* kWorkshopRole = "zengine.workshop";

/// THE PROMPT, in columns: its mark and a space before the editable text -- a fact about a ROW this
/// pane composes rather than about any screen. Every mark the `prompt` setting takes is one column.
constexpr std::int64_t kPromptCols = 2;

/// THE MARK THE LINE IS TYPED AFTER unless a layout keeps another, and the marks it takes.
constexpr const char* kPrompt = ">";
const std::vector<std::string> kPromptMarks = {">", "$", "%", "#", ":"};

/// ROWS OF THE RECORD A WHEEL NOTCH READS unless a layout keeps another, and the most and fewest.
constexpr std::int64_t kWheelRows = 3;
constexpr std::int64_t kWheelRowsLow = 1;
constexpr std::int64_t kWheelRowsHigh = 24;

/// The record wrapped into rows, which `view_top` reads: defined in `view.hpp`.
struct WrappedRecord;

/// A transcript value a row carries: the terminal instance and the observation.
using Subject = std::pair<std::int64_t, std::int64_t>;

/// The pane's room, its rows and their presses; a transcript row carrying a value means it.
using Rows = ws::PaneRowsFacet<pane::kTerminalPane, pane::kTerminalPaneRole,
                               zengine::manual::pages::zengine_terminal::kManual, Subject>;
using Actions = ws::PaneActionFacet<pane::kTerminalPane, pane::kTerminalPaneRole>;
using Menu = ws::PaneMenuFacet<pane::kTerminalPane, pane::kTerminalPaneRole>;
/// Its row's settings, asked of the layouts' owner when it arrives; no room is drawn before.
using Settings = ws::PaneSettingsFacet<pane::kTerminalPane, pane::kTerminalPaneRole>;

class TerminalPaneWeave
    : public zengine::With<Rows, Actions, Menu, Settings>::Weave<
          TerminalPaneWeave, pane::TerminalPaneState,
          loom::Accept<ws::TerminalValueAnswered, ws::PaneCarryAnswered,
                       loom::DispatchRefused, PaneKey, PaneTextInput, TranscriptShown,
                       TerminalActed, TerminalCompletionOffered, gfx::ClipboardCopy,
                       gfx::ClipboardText>,
          loom::Emit<ws::v2::PaneSettingsDeclared, ws::v4::PaneActions, PaneEscapeUnspent,
                     ws::TerminalValueRequested,
                     ws::PaneValueCarryRequested, TerminalActRequested,
                     TerminalCompletionRequested, gfx::ClipboardCopy,
                     gfx::ClipboardTextRequested>> {
public:
    /// THE LINE A RELOAD KEPT, PUT BACK -- with the caret at its end, which is where the
    /// completer requires it and where a weaver about to keep typing wants it -- then the offer.
    void on_activated(loom::Mail& mail) {
        line_.set(state_.line, state_.line.size());
        room().announce(*this, mail);
    }

    PaneOffered offering() const {
        return PaneOffered{pane::kTerminalPane, pane::kTerminalPaneName,
                           pane::kTerminalPaneSummary, 10, 72};
    }

    /// The rows the pane answers to, and the settings it takes.
    void offered(loom::Mail& mail) {
        declare(mail);
        (void)mail.as_role(pane::kTerminalPaneRole)
            .send_to_role(ws::kLayoutsRole,
                          ws::v2::PaneSettingsDeclared{pane::kTerminalPane, setting_rows()});
    }

    /// A room granted, prose or canvas: the pane draws its rows there.
    void on_room(loom::Mail& mail) { say(mail); }

    /// THE SETTINGS THIS LAYOUT KEEPS FOR THE PANE, as the layouts' owner handed them and its
    /// facet took them whole: a list that keeps none is every setting at its default. A value a
    /// setting cannot use leaves its default in force and was said back to the owner -- the
    /// legend's on its own row too; a key it did not declare is passed over. Held as plain members,
    /// never reload state.
    void on_settings(const ws::SettingsTaken& taken, loom::Mail& mail) {
        legend_shown_ = taken.flag(pane::kSettingLegend);
        const std::string refused = taken.refusal(pane::kSettingLegend);
        legend_problem_ = refused.empty() ? std::string() : refused + " -- " + ws::kSettingOn;
        wheel_rows_ = taken.number(pane::kSettingWheelRows);
        prompt_ = taken.text(pane::kSettingPrompt);
        say(mail);
    }

    /// THE SETTINGS THE PANE TAKES, each with its default, how a weaver finds it and what it does:
    /// the legend, pinned; the wheel's rows a notch; the prompt's mark.
    static std::vector<ws::v2::PaneSettingRow> setting_rows() {
        ws::v2::PaneSettingRow legend;
        legend.key = pane::kSettingLegend;
        legend.flag = true;
        legend.tags = {"chrome", "help"};
        legend.pinned = true;
        legend.summary = "show the shortcut legend bar";
        ws::v2::PaneSettingRow wheel;
        wheel.key = pane::kSettingWheelRows;
        wheel.number = kWheelRows;
        wheel.low = kWheelRowsLow;
        wheel.high = kWheelRowsHigh;
        wheel.tags = {"input", "scroll"};
        wheel.summary = "rows scrolled per mouse wheel notch";
        ws::v2::PaneSettingRow prompt;
        prompt.key = pane::kSettingPrompt;
        prompt.text = kPrompt;
        prompt.choices = kPromptMarks;
        prompt.tags = {"chrome", "text"};
        prompt.summary = "input line prompt mark";
        return {legend, wheel, prompt};
    }


    /// WHAT THE PARTICIPANT'S RECORD HOLDS, SAID BY THE HOST. Replaced WHOLE, never merged --
    /// this pane keeps no second copy of a transcript beyond the picture it was last shown.
    void on(const TranscriptShown& said, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return; // a stranger's opinion about a record is not a reading of one
        }
        known_ = said;
        heard_ = true;
        // THE ENTRY BEING READ WAS EVICTED: the view moves to the oldest entry kept, and says why
        // at its top until the weaver scrolls again. Nothing else in a new picture moves the view.
        if (!reading_.follow && reading_.seq < known_.dropped) {
            reading_.seq = known_.dropped;
            reading_.row = 0;
            reading_.lost = true;
        }
        say(mail);
    }

    /// THE WHEEL READS THE RECORD: the layout's `wheel-rows` a notch, fractions carried, +1 away
    /// from the weaver being older output. Reading moves no line, recall, list or notice.
    void wheel(double dy, loom::Mail& mail) {
        wheel_ += dy * static_cast<double>(wheel_rows_);
        const std::int64_t rows = static_cast<std::int64_t>(wheel_);
        wheel_ -= static_cast<double>(rows);
        if (rows == 0) {
            return;
        }
        scroll(-rows);
        say(mail);
    }

    /// WHAT THE PARTICIPANT MADE OF THE LINE. An accepted act says nothing here: the line
    /// was recorded on the transcript, so the new PICTURE arrives on the same drain and is
    /// the answer a weaver reads. A refusal is the door's own words and belongs beside the
    /// line it is about.
    void on(const TerminalActed& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !acting_ || mail.correlation() != act_pending_) {
            return;
        }
        acting_ = false;
        if (!said.accepted) {
            notice_ = said.refusal;
        }
        say(mail);
    }

    /// What could be said next: the answer is the participant's, and which candidate the weaver
    /// is on is this pane's, surviving a recomputation on its own rule. First, is it still about
    /// this line? Between ask and answer a weaver can empty the line, move the caret off its end,
    /// submit or type, and a list for a word nobody is typing would splice a stale `partial`.
    void on(const TerminalCompletionOffered& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !completing_ || mail.correlation() != completion_pending_) {
            return;
        }
        completing_ = false;
        if (asked_intent_ != intent_ || asked_about_ != here()) {
            // THE INTENT THIS WAS ABOUT IS GONE -- an edit, a clear, a recall, a dismissal or
            // a lock came between, even if the line's bytes match again. The answer is dropped
            // whole: not shown, so it cannot be read as being about this line, and not held,
            // so it cannot be accepted into one. It is asked again only when the intent
            // standing now wanted a list; a lock and a recall did not.
            if (wanted_ && wanted_intent_ == intent_) {
                ask_completion(mail);
            }
            say(mail);
            return;
        }
        // The selection survives a recomputation, not a change of question: the question is the
        // slot and the partial together, and a different word or part of the line is a new list
        // starting at the top. Clamped either way, since a list can shrink under the same
        // partial; resetting on every recomputation made the arrow keys appear to do nothing.
        const bool same_question = offered_.open && said.open && said.slot == offered_.slot &&
                                   said.partial == offered_.partial;
        offered_ = said;
        offered_about_ = asked_about_;
        offered_intent_ = asked_intent_;
        if (same_question && !offered_.candidates.empty()) {
            const std::size_t last = offered_.candidates.size() - 1;
            selected_ = selected_ < last ? selected_ : last;
        } else {
            unmove_cursor();
        }
        // A DISMISSAL BELONGS TO THE PART OF THE LINE IT WAS MADE IN. Moving on to the next
        // word is a new question, so the list comes back for it.
        if (dismissed_ && offered_.slot != dismissed_at_) {
            dismissed_ = false;
        }
        say(mail);
    }

    /// A PRESS ON THE CANVAS, or in a graphics window, acts only while the picture it was aimed at
    /// was drawn under what the transcript's rows mean now -- on a value it picks the value up;
    /// elsewhere, and on a part a window asked as an action, where no carry begins, it means what a
    /// press on that row means. Motion, release and loss are nothing: no sweep here, and a pickup's
    /// drag is Workshop's once the carry begins.
    void press(const ws::RowPress& at, loom::Mail& mail) {
        if (at.moved) { // the window landed it on a row the transcript has since replaced there
            notice_ = "That transcript picture changed; try again";
            say(mail);
            return;
        }
        if (const auto* subject = at.window ? nullptr : room().map().at(at.row, at.column)) {
            acquire(*subject, true, mail);
            return;
        }
        press(at.row, at.column, mail);
    }


    /// A RIGHT PRESS on a value offers to pick up a copy of it; anywhere else, or while a pickup
    /// is in flight, it is handed back, so Workshop's own pane menu opens where it was made.
    bool right_press(const Subject& subject, const ws::RowPress& at, loom::Mail& mail) {
        if (capture_.pending()) { // a pickup in flight offers nothing now: handed back, not dropped
            return false;
        }
        menu_subject_ = subject;
        menu().send(mail, menu().offer("Transcript value")
                              .at(at.row, at.column)
                              .row("copy", "Pick up a copy"));
        return true;
    }
    void on_menu(const std::string& id, const std::string&, loom::Mail& mail) {
        if (id == "copy") acquire(menu_subject_, false, mail);
    }
    void acquire(Subject subject, bool drag, loom::Mail& mail) {
        if (capture_.pending()) {
            notice_ = "A transcript pickup is pending";
            say(mail);
            return;
        }
        capture_gesture_ = mail.correlation();
        capture_drag_ = drag;
        const bool queued = capture_.send_to_role(mail.as_role(pane::kTerminalPaneRole), kWorkshopRole,
            ws::TerminalValueRequested{pane::kTerminalPane, subject.first, subject.second,
                                       static_cast<std::int64_t>(capture_gesture_)}, ++asked_);
        notice_ = queued ? "Picking up transcript value" : "Transcript pickup could not be queued";
        say(mail);
    }
    void on(const ws::TerminalValueAnswered& answer, loom::Mail& mail) {
        if (!capture_.is<ws::TerminalValueRequested>() || !capture_.matches_answer(mail)) return;
        capture_.forget();
        if (!answer.available) { notice_ = answer.reason; say(mail); return; }
        const bool queued = capture_.send_to_role(mail.as_role(pane::kTerminalPaneRole), kWorkshopRole,
            ws::PaneValueCarryRequested{pane::kTerminalPane, answer.label, answer.pair, capture_drag_},
            capture_gesture_);
        notice_ = queued ? "Placing transcript copy" : "Transcript copy could not be queued";
        say(mail);
    }
    void on(const ws::PaneCarryAnswered& answer, loom::Mail& mail) {
        if (!capture_.is<ws::PaneValueCarryRequested>() || !capture_.matches_answer(mail)) return;
        capture_.forget();
        notice_ = answer.carried ? std::string() : answer.reason;
        say(mail);
    }
    void on(const loom::DispatchRefused& answer, loom::Mail& mail) {
        if (!capture_.matches_refusal(answer, mail)) return;
        capture_.forget();
        notice_ = "Transcript pickup refused: " + answer.reason;
        say(mail);
    }

    /// A PRESS NAMES A ROW OF THIS PANE'S ROOM.
    ///
    /// THREE ROWS MEAN SOMETHING: a candidate row chooses that candidate, the input row places
    /// the caret, and the row below the view follows the newest output again. Everything else is
    /// consumed and changes nothing, which is what a pane that owns visible room owes the cells it
    /// is not using.
    void press(std::int64_t row, std::int64_t column, loom::Mail& mail) {
        if (row == input_row_ && input_row_ >= 0) {
            notice_.clear();
            history_note_.clear();
            // A PRESS ON THE LINE IS AN EDITING ACT, so a recalled line becomes the draft here
            // and the press then means only what it always means.
            settle_recall();
            const std::size_t was = line_.caret();
            const bool had_selection = line_.has_selection();
            // THROUGH THE WINDOW THE ROW WAS DRAWN WITH. A visible column names
            // `first_visible + offset` of the WHOLE authored line, never the offset alone --
            // that is the one subtraction a horizontal viewport adds to a hit test.
            const std::int64_t offset = column - kPromptCols;
            const std::size_t target =
                line_.position_at_column(offset < 0 ? 0 : offset);
            // ...AND A SECOND PRESS IN THE SAME WORD SELECTS IT; the first press still places
            // the caret.
            if (word_press_ && target == was && was == word_press_at_) {
                line_.select_word_at(target);
                word_press_ = false;
            } else {
                line_.place(target);
                word_press_ = true;
                word_press_at_ = line_.caret();
            }
            if (line_.caret() != was || had_selection || line_.has_selection()) {
                moved();
                ask_completion(mail); // the caret moving changes whether completion may ask
            }
            say(mail);
            return;
        }
        word_press_ = false;
        // THE ROW BELOW THE VIEW IS THE WAY BACK TO THE NEWEST OUTPUT; the row above it says what
        // is above and is consumed like the rest of the record.
        if (row == bottom_marker_row_ && bottom_marker_row_ >= 0) {
            reading_ = Reading{};
            say(mail);
            return;
        }
        if (list_first_row_ >= 0 && row >= list_first_row_ &&
            row < list_first_row_ + list_row_count_) {
            // ROW 0 OF THE LIST IS THE HEADING and is not a candidate. A press on it is a
            // press on the list -- consumed, changing nothing.
            const std::int64_t at = row - list_first_row_ - 1;
            if (at < 0) {
                return;
            }
            // ONE SELECTION, WHICHEVER HAND MOVED IT: this writes the fields Up/Down write,
            // so the row a click chooses is a row the completion key, and Enter, then take.
            const std::size_t index = list_shown_first_ + static_cast<std::size_t>(at);
            if (index < offered_.candidates.size()) {
                selected_ = index;
                cursor_moved_ = true;
                say(mail);
            }
        }
    }

    /// THE LINE'S OWN VOCABULARY: the editing keys, selection, clipboard and word movement,
    /// all through the one component call every consumer makes. A gesture this pane declared
    /// a row for never reaches here -- it arrives as its id instead (`on_action`).
    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || key.pane != pane::kTerminalPane) {
            return;
        }
        word_press_ = false;
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (!line_.consume(key.scancode, key.modifiers, clip_)) {
            return; // a key the line does not take is no act, and ends no recall
        }
        notice_.clear();
        history_note_.clear();
        // A KEY THE LINE TOOK ENDS A RECALL, and it has already done its one piece of work on
        // the recalled line -- which is the draft from here on.
        settle_recall();
        if (clip_.writes != copied_before) {
            mail.publish(gfx::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_paste(mail);
        }
        // AN EDIT OR A CARET MOVE CHANGES WHETHER THE COMPLETER MAY BE ASKED, and what it
        // would answer, so both reach the ask.
        moved();
        remember_line();
        ask_completion(mail);
        say(mail);
    }

    void on(const PaneTextInput& typed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || typed.pane != pane::kTerminalPane) {
            return;
        }
        if (typed.text.empty() || !admissible(typed.text)) {
            return;
        }
        word_press_ = false;
        notice_.clear();
        history_note_.clear();
        settle_recall(); // typing onto a recalled line makes it the draft, typed into once
        // AT THE CARET, WHICH IS NOT ALWAYS THE END. `type` is the only door that moves the
        // text and the caret together, so a keystroke in the middle of a line cannot leave
        // one behind. Typing IS the completion gesture.
        line_.type(typed.text);
        asked_for_list_ = false;
        moved();
        remember_line();
        ask_completion(mail);
        say(mail);
    }

    // The rows it declared, acted on: defined at the end of this file, after `view.hpp`.
    void on_action(const std::string& id, loom::Mail& mail);

    /// AN ID THIS PANE NEVER DECLARED IS NO ACT (`agents/panes.md`), so it spends nothing: not
    /// the notice, and not the memory that lets a second press in a word select it.
    static bool answers(const std::string& id) {
        for (const PaneActionRow& row : action_rows()) {
            if (row.id == id) {
                return true;
            }
        }
        return false;
    }

    void on(const gfx::ClipboardCopy& said, loom::Mail&) {
        // WHAT THE PROCESS SAYS IT COPIED -- this pane's own copies included, which is why
        // it is a mirror rather than a second store: a paste answers with this.
        clip_.text = said.text;
    }

    /// The window's answer to a paste this pane asked for: the one road foreign clipboard text has
    /// into this line, under a weaver's own gesture. The correlation says which ask, not that the
    /// line that asked still exists: text asked for by a draft that has ended lands nowhere
    /// (WL-TEXT-09). It goes in through `TextBox::paste` -- one undo entry, `pasteable_line`
    /// flattening tabs and line breaks -- and the gate judges the text that would land: what is
    /// still undrawable is refused whole, aloud, on the notice row.
    void on(const gfx::ClipboardText& a, loom::Mail& mail) {
        if (!mail.answers_ask() || !paste_.awaiting || mail.correlation() != paste_.pending) {
            return;
        }
        paste_.awaiting = false;
        if (line_.draft_epoch() != paste_.epoch) {
            return; // the draft that asked is over; the payload is discarded, silently
        }
        // A MEDIUM THAT CANNOT BE READ FALLS BACK TO THE MIRROR (WL-TEXT-10), which is what
        // keeps copy-here paste-there true on a terminal that claims nothing.
        const std::string offered = a.readable ? a.text : clip_.text;
        if (!admissible(component::pasteable_line(offered))) {
            notice_ = "the clipboard holds bytes outside plain ASCII, which this line cannot "
                      "carry truthfully -- nothing was pasted";
            say(mail);
            return;
        }
        if (a.readable) {
            // The platform's current truth, asked for by THIS paste -- including an empty
            // one, which is a real answer and must not leave a stale mirror to paste from.
            clip_.text = a.text;
        }
        line_.paste(clip_);
        moved();
        remember_line();
        ask_completion(mail);
        say(mail);
    }

private:
    // ---- Offering and declaring ---------------------------------------------------------

    /// What this pane answers to: ten rows, and they never change. No modes, unlike Info's and
    /// Files': the line is always open (it is the pane), so each id names one gesture whose
    /// meaning the pane resolves against its state (a recall, a list, a line), and every other
    /// key reaches the line as a `PaneKey`. Declared as v4, so the early submit names its key for
    /// a POSIX terminal, which cannot tell Shift on Return.
    void declare(loom::Mail& mail) {
        (void)mail.as_role(pane::kTerminalPaneRole)
            .send_to_role(kWorkshopRole,
                          ws::v4::PaneActions{pane::kTerminalPane, {}, action_rows(), {}});
    }

    /// THE ROWS -- what `declare` tells Workshop, and what `answers` reads, so what the pane
    /// acts on and what it said it acts on are one list -- each with what its press may send
    /// beside the pane protocol: the line, to be acted on, and the ask for what could follow it.
    static std::vector<PaneActionRow> action_rows() {
        std::vector<PaneActionRow> rows;
        const auto row = [&rows](const char* id, const char* label, std::int64_t sc,
                                 std::vector<ws::ActionCause> causes = {}) {
            rows.push_back(PaneActionRow{id, label, sc, input::mod::kNone, {}, std::move(causes)});
        };
        row(pane::kActionSubmit, "run, take moved row, keep recall", input::scan::kReturn,
            {ws::cause_of<TerminalActRequested>()});
        rows.push_back(PaneActionRow{pane::kActionSubmitEarly, "run the line as typed",
                                     input::scan::kReturn, input::mod::kShift, {},
                                     {ws::cause_of<TerminalActRequested>()}, {}, input::scan::kE,
                                     input::mod::kCtrl});
        row(pane::kActionComplete, "what can this terminal say?", input::scan::kTab,
            {ws::cause_of<TerminalCompletionRequested>()});
        row(pane::kActionUp, "older command / list up", input::scan::kUp);
        row(pane::kActionDown, "newer command / list down", input::scan::kDown);
        row(pane::kActionBack, "back: recall, list, line, desk", input::scan::kEscape);
        const auto chord = [&rows](const char* id, const char* label, std::int64_t sc) {
            rows.push_back(PaneActionRow{id, label, sc, input::mod::kCtrl});
        };
        chord(pane::kActionScrollUp, "read older output", input::scan::kUp);
        chord(pane::kActionScrollDown, "read newer output", input::scan::kDown);
        chord(pane::kActionOldest, "oldest kept output", input::scan::kHome);
        chord(pane::kActionNewest, "back to the newest output", input::scan::kEnd);
        return rows;
    }

    // ---- The one act and the one read ----------------------------------------------------

    void submit(loom::Mail& mail) {
        const std::string line = line_.text();
        line_.clear();
        // A SUBMITTED LINE ENDS BOTH PIECES OF COMPLETION STATE. Escape said "not for this
        // word" and the completion key said "show me anyway"; the next line is neither.
        dismissed_ = false;
        asked_for_list_ = false;
        offered_ = TerminalCompletionOffered{};
        unmove_cursor();
        moved();
        remember_line();
        // A SUBMITTED LINE IS READ WHERE ITS OUTPUT ARRIVES: the view follows the newest again.
        reading_ = Reading{};
        if (line.empty()) {
            say(mail);
            return;
        }
        act_pending_ = ++asked_;
        acting_ = true;
        (void)mail.as_role(pane::kTerminalPaneRole)
            .send_to_role(kWorkshopRole, TerminalActRequested{ws::kTerminalSubmitAct, line},
                          act_pending_);
        say(mail);
    }

    /// THE PASTE'S ASK, and the draft that asked, so the answer is measured against the draft it comes back
    /// to (WL-TEXT-09): `set` and `clear` end a draft and bump this counter, so an edit is the
    /// same draft and an abandoned or submitted line is not.
    void begin_paste(loom::Mail& mail) {
        paste_.pending = ++asked_;
        paste_.epoch = line_.draft_epoch();
        paste_.awaiting = true;
        (void)mail.as_role(pane::kTerminalPaneRole)
            .send_to_role(room().clipboard_office(), gfx::ClipboardTextRequested{},
                          paste_.pending);
    }

    // ---- The line: the one place it is kept, and whether a command is composed --------------

    /// THE ONE PLACE THE KEPT LINE IS WRITTEN. `state_` is what a same-shape reload carries,
    /// so it is updated wherever the text changes and nowhere else. A recalled line is browsed
    /// and not yet adopted, so while one is on the line the kept line is the draft before it.
    void remember_line() { state_.line = recall_.active ? recall_.before.text() : line_.text(); }

    /// NO COMMAND IS BEING COMPOSED: the line is empty and the weaver has not asked for a list
    /// on it. This, or a recall already under way, is when Up and Down mean history.
    bool composing_nothing() const { return line_.empty() && !asked_for_list_; }

    // ---- Command history, defined in `history_and_completion.hpp` ---------------------------

    /// ONE COMMAND A WEAVER CAN RECALL, named by its place in the record's whole history.
    struct Recallable {
        std::int64_t seq = 0;
        std::string text;
    };

    std::vector<Recallable> recallable() const;
    void recall(int by, loom::Mail& mail);
    void show_recalled(const Recallable& c);
    bool settle_recall();
    void cancel_recall();
    std::string history_heading() const;

    // ---- Completion: the one read and the list's cursor, in `history_and_completion.hpp` ----

    /// THE LINE A QUESTION ABOUT COMPLETION IS ABOUT. Two of these are kept: what the
    /// OUTSTANDING ask was about (`asked_about_`), and what the offer in hand is about
    /// (`offered_about_`).
    struct Asking {
        std::string line;
        std::size_t caret = 0;
        bool operator==(const Asking& o) const { return caret == o.caret && line == o.line; }
        bool operator!=(const Asking& o) const { return !(*this == o); }
    };

    void ask_completion(loom::Mail& mail);
    Asking here() const;
    bool offer_applies() const;
    void moved();
    bool selectable() const;
    void move_selection(int by);
    void accept_candidate();
    void take_candidate(loom::Mail& mail);
    void unmove_cursor();

    // ---- Reading the record, defined in `view.hpp` ------------------------------------------

    std::int64_t view_top(const WrappedRecord& record, std::int64_t view) const;
    void scroll(std::int64_t by);
    std::int64_t page() const;

    // ---- The rows, and the caret in them: composed in `view.hpp`, named here ----------------

    void say(loom::Mail& mail);
    void send(std::vector<zengine::workshop::TextRow> out, std::vector<ws::RowPart> parts,
              loom::Mail& mail);
    template <typename Push> void say_list(std::size_t room, const Push& push);
    ws::RowsCaret caret() const;

    /// WHAT THE TERMINAL CALLS ITS PARTS, of the `said` rows it sends: a transcript entry carrying
    /// a value, `entry:<observation>`; the line being typed, `line`; the way back to the newest
    /// output, `newest`; a completion candidate, `candidate:<what it says>`; and the legend row,
    /// `legend`, which a layout's setting shows or hides.
    std::vector<ws::RowPart> named_rows(std::int64_t said) const {
        ws::PartNames<ws::RowPart> names;
        const auto name = [&](std::string what, std::int64_t row) {
            if (row >= 0 && row < said) {
                (void)names.add(ws::RowPart{std::move(what), row, 0, columns(), true});
            }
        };
        for (const auto& span : room().map().spans()) {
            name("entry:" + std::to_string(span.meaning.second), span.row);
        }
        name("line", input_row_);
        name("newest", bottom_marker_row_);
        name(pane::kSettingLegend, legend_row_);
        for (std::int64_t i = 1; i < list_row_count_; ++i) {
            const std::size_t index = list_shown_first_ + static_cast<std::size_t>(i - 1);
            if (list_first_row_ >= 0 && index < offered_.candidates.size()) {
                name("candidate:" + offered_.candidates[index].display, list_first_row_ + i);
            }
        }
        return names.take();
    }

    // ---- State not in the shape --------------------------------------------------------

    Rows& room() { return this->template facet<Rows>(); }
    const Rows& room() const { return this->template facet<Rows>(); }
    /// The rows and columns the pane composes for (`PaneFacet`).
    std::int64_t rows() const { return room().rows(); }
    std::int64_t columns() const { return room().columns(); }
    /// This image's one outstanding menu (`PaneMenuFacet`).
    Menu& menu() { return this->template facet<Menu>(); }

    /// THE HOST'S LAST READING OF THE PARTICIPANT'S RECORD. Replaced whole; owned by nobody
    /// here; deliberately NOT in the state shape.
    TranscriptShown known_;
    bool heard_ = false;

    /// A recalled command being browsed, and the line as it stood before the first recall.
    /// Browsing is not composing: Up and Down still walk the record, and Enter or Tab lock the
    /// line in. `seq` names the entry by its place in the whole history (`dropped` + index),
    /// which eviction does not move; `before` is the whole box, so walking back past the newest
    /// command, or cancelling, returns exactly the draft that was there.
    struct Recall {
        bool active = false;
        std::int64_t seq = 0;
        component::TextBox before;
    };
    Recall recall_;

    /// WHAT THE ROW ABOVE THE LINE SAYS ABOUT HISTORY, when it says anything: the position
    /// while browsing, or why Up or Down did nothing. Cleared by the weaver's next act.
    std::string history_note_;

    /// WHERE THE WEAVER IS READING THE RECORD. Following the newest output unless they scrolled
    /// away; then the top row of the view is anchored to an entry by its place in the record's
    /// whole history and a wrapped row inside it -- so new output leaves the view where it is, a
    /// resize re-wraps under the same entry, and an eviction moves the view only when the entry
    /// being read is itself gone (`lost`, said at the top until the next scroll).
    struct Reading {
        bool follow = true;
        std::int64_t seq = 0;
        std::int64_t row = 0;
        bool lost = false;
    };
    Reading reading_;
    /// The wheel's notches not yet worth a row.
    double wheel_ = 0.0;
    /// HOW TALL THE TRANSCRIPT'S VIEW WAS WHEN LAST SAID -- what a page step is measured in -- and
    /// where its two markers were, so a press reads the picture it was aimed at.
    std::int64_t view_rows_ = 0;
    std::int64_t top_marker_row_ = -1;
    std::int64_t bottom_marker_row_ = -1;

    /// WHICH INTENT THE LINE IS IN. Bumped by every act that changes what a completion would
    /// be about -- an edit, a caret move, a dismissal, a clear, a submit, each step of a recall
    /// and its lock -- so an answer is taken only by the intent that asked for it, even when
    /// the line's bytes happen to match again.
    std::uint64_t intent_ = 0;

    /// THE LINE BEING TYPED -- and the caret in it, the selection, and which part of it the
    /// row is showing. Its TEXT is the one thing a reload keeps.
    component::TextBox line_;

    /// WHAT THE PARTICIPANT SAID COULD COME NEXT, and which of it the weaver is standing on.
    TerminalCompletionOffered offered_;
    Asking offered_about_;
    std::uint64_t offered_intent_ = 0;
    std::size_t selected_ = 0;
    /// WHETHER THE WEAVER MOVED THE LIST'S CURSOR -- by Up, Down or a press on a candidate's row
    /// -- since the list put it on its first row: what makes Enter take the candidate it is on.
    bool cursor_moved_ = false;
    bool dismissed_ = false;
    std::string dismissed_at_;
    bool asked_for_list_ = false;

    component::Clipboard clip_;
    struct Paste {
        bool awaiting = false;
        std::uint64_t pending = 0;
        /// The draft that asked (`component::TextBox::draft_epoch`): the correlation says this
        /// answer is to this pane's own ask, and the epoch that the line it asked for still exists.
        std::uint64_t epoch = 0;
    };
    Paste paste_;

    std::string notice_;

    /// THE SETTINGS THIS LAYOUT HANDED, and why a legend value it could not use left it shown --
    /// said on the legend row itself, which no keystroke clears. Plain members: a reloaded image
    /// asks for them again, and draws no room before it holds them.
    bool legend_shown_ = true;
    std::string legend_problem_;
    std::int64_t wheel_rows_ = kWheelRows;
    std::string prompt_ = kPrompt;
    std::int64_t legend_row_ = -1;

    /// WHICH COMPOSED ROW IS THE INPUT LINE, and where the list is -- written by `say` and
    /// read by a press, so a press and a picture cannot disagree about what is where.
    std::int64_t input_row_ = -1;
    /// The line as the input row last showed it: its slice, caret and selection from one window.
    component::TextBoxCells line_cells_;
    std::int64_t list_first_row_ = -1;
    std::int64_t list_row_count_ = 0;
    std::size_t list_shown_first_ = 0;

    /// A PRESS THAT LANDED WHERE THE CARET ALREADY WAS, so the next one selects the word.
    bool word_press_ = false;
    std::size_t word_press_at_ = 0;

    Subject menu_subject_{};
    loom::RoleRequest capture_;
    std::uint64_t capture_gesture_ = 0;
    bool capture_drag_ = false;
    /// ONE COUNTER FOR EVERY QUESTION THIS PANE ASKS, so a correlation is this incarnation's
    /// own and an answer to somebody else's question is not mistaken for one to ours.
    std::uint64_t asked_ = 0;
    bool acting_ = false;
    std::uint64_t act_pending_ = 0;
    bool completing_ = false;
    std::uint64_t completion_pending_ = 0;
    Asking asked_about_;
    std::uint64_t asked_intent_ = 0;
    /// THE INTENT THAT LAST WANTED A LIST. An answer that arrives for an older intent is
    /// dropped, and asked again only if the intent standing now wants one -- a lock, a recall
    /// and a submit do not.
    std::uint64_t wanted_intent_ = 0;
    bool wanted_ = false;

};

} // namespace

#include "terminal/history_and_completion.hpp"
#include "terminal/view.hpp"

namespace {

// THE ROWS THE PANE DECLARED, ACTED ON, each an id `answers` admitted. Defined after the class and
// its headers because Ctrl+Home measures the whole record as `view.hpp` wraps it. Every id the
// pane declares says its rows below, so what an act spends is always published.
void TerminalPaneWeave::on_action(const std::string& id, loom::Mail& mail) {
    // THE READING KEYS MOVE THE VIEW AND NOTHING ELSE: not the line, a recall, a list, a
    // refusal beside the line or the memory that makes a second press select a word.
    if (id == pane::kActionScrollUp || id == pane::kActionScrollDown ||
        id == pane::kActionOldest || id == pane::kActionNewest) {
        if (id == pane::kActionScrollUp) {
            scroll(-page());
        } else if (id == pane::kActionScrollDown) {
            scroll(page());
        } else if (id == pane::kActionOldest) {
            scroll(-wrap_record(known_.entries, columns()).total());
        } else {
            reading_ = Reading{};
        }
        say(mail);
        return;
    }
    word_press_ = false;
    notice_.clear();
    history_note_.clear();
    // (!) ENTER, SHIFT+ENTER AND TAB ON A RECALLED LINE LOCK IT IN AND DO NOTHING ELSE. The
    // press is spent whole on ending the recall: no submission, no candidate taken, and no list
    // asked for. The next press has its ordinary meaning.
    if (id == pane::kActionSubmit || id == pane::kActionSubmitEarly ||
        id == pane::kActionComplete) {
        if (settle_recall()) {
            remember_line();
            say(mail);
            return;
        }
    }
    // ENTER TAKES THE CANDIDATE THE WEAVER MOVED THE LIST'S CURSOR TO, as Tab does; with the
    // cursor where the list put it, it runs the line. Shift+Enter runs the line as typed.
    if (id == pane::kActionSubmit && cursor_moved_ && selectable()) {
        take_candidate(mail);
        say(mail);
        return;
    }
    if (id == pane::kActionSubmit || id == pane::kActionSubmitEarly) {
        submit(mail);
        return;
    }
    // UP AND DOWN WALK THE HISTORY WHEN NO COMMAND IS BEING COMPOSED, and keep walking it
    // while a recalled line is browsed; on a command being composed they move the list.
    if (id == pane::kActionUp || id == pane::kActionDown) {
        const int by = id == pane::kActionUp ? -1 : +1;
        if (recall_.active || composing_nothing()) {
            recall(by, mail);
            return;
        }
        move_selection(by);
        say(mail);
        return;
    }
    if (id == pane::kActionComplete) {
        // ONE KEY, ONE MEANING: "help me here". With a list on screen that is taking the
        // selected candidate; with nothing on screen it is asking for one, which is the
        // only gesture discovery needs because every other entry point is typing.
        if (selectable()) {
            take_candidate(mail);
        } else {
            asked_for_list_ = true;
            dismissed_ = false;
            moved();
            ask_completion(mail);
        }
        say(mail);
        return;
    }
    if (id == pane::kActionBack) {
        if (recall_.active) {
            // ESCAPE ON A RECALLED LINE GOES BACK TO THE LINE BEFORE THE RECALL, whole.
            cancel_recall();
        } else if (selectable()) {
            // THE LIST GOES AWAY AND THE LINE IS UNTOUCHED. A weaver who wanted the line
            // gone presses it again; a weaver who wanted only the list gone has not lost
            // the word they were half-way through.
            dismissed_ = true;
            dismissed_at_ = offered_.slot;
            asked_for_list_ = false;
            cursor_moved_ = false; // a list brought back is one the weaver has not moved in
            moved();
        } else if (!composing_nothing()) {
            // ABANDONING THE LINE ABANDONS THE DISMISSAL WITH IT. The dismissal was made
            // against a word; there is no longer a word, so keeping it would leave the
            // list hidden for the whole of the next command with nothing on screen to
            // explain why.
            line_.clear(); // ...and the caret with it: `clear` moves both
            dismissed_ = false;
            asked_for_list_ = false;
            moved();
            remember_line();
            ask_completion(mail);
        } else {
            // Nothing more specific is left (no recall, list or line): this Escape was
            // unspent here, and saying so lets Workshop's own last meaning for it -- putting
            // this pane down -- run if it is still the latest gesture. Echoed under the number
            // it arrived on, since the answer may reach Workshop behind later gestures.
            (void)mail.as_role(pane::kTerminalPaneRole)
                .send_to_role(kWorkshopRole, PaneEscapeUnspent{pane::kTerminalPane},
                              mail.correlation());
        }
        say(mail);
    }
}

} // namespace

ZEN_EXPORT_WEAVE(TerminalPaneWeave)
