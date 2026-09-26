// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Terminal pane: a loadable weave that offers Workshop one pane -- the record of the
// terminal participant this host mounted, and the line a maker composes messages on. The
// participant stays with the host (no message in its interface drives it), so what crosses is a
// picture the host derives (`TranscriptShown`), one act (`TerminalActRequested`) and one read
// (`TerminalCompletionRequested`): nothing here can speak as that participant, only ask it to.
// Workshop law: agents/workshop/terminal-pane.md

// The line's caret and selection are published beside the rows (`PaneCaret`). This pane
// ignores `PaneDragged`, so a sweep is not a gesture here: a press places the caret, a second
// press in a word selects it, and shift with the caret keys sweeps by keyboard.

#include "terminal-pane/vocabulary.hpp"

#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"

#include "activation/activation.hpp"
#include "component/text_box.hpp"
#include "component/row_map.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_menu.hpp"
#include <zen/weave/dispatch_refusal.hpp>
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/role_request.hpp>
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
namespace pane = zengine::terminal_pane;

using ws::PaneActionRequested;
using ws::PaneActionRow;
using ws::PaneActions;
using ws::PaneCaret;
using ws::PaneCatalogRequested;
using ws::PaneContent;
using ws::PaneEscapeUnspent;
using ws::PaneKey;
using ws::v2::PaneOffered;
using ws::PanePressed;
using ws::PaneRoom;
using ws::PaneTextInput;
using ws::PaneWheel;
using ws::ShownCandidate;
using ws::ShownEntry;
using ws::TerminalActed;
using ws::TerminalActRequested;
using ws::TerminalCompletionOffered;
using ws::TerminalCompletionRequested;
using ws::TranscriptShown;
using zengine::workshop::pane_text::admissible;
using zengine::workshop::pane_text::drawable;
using zengine::workshop::pane_text::fit;
using zengine::workshop::pane_text::wrap;

constexpr const char* kWorkshopRole = "zengine.workshop";

/// THE PROMPT, in columns: the `> ` before the editable text -- a fact about a ROW this pane
/// composes rather than about any screen.
constexpr std::int64_t kPromptCols = 2;

/// ONE COLUMN OF THE INPUT ROW THE TEXT MAY NOT USE -- the caret's own. A caret at the end
/// of a full row would otherwise sit one past the room, and in a cell projection the mark is
/// a character that needs a cell of its own.
constexpr std::int64_t kCaretCols = 1;

/// The chrome a pane spends on being this pane, whatever is in it: the header, the standing
/// legend, the omission marker and the input row.
constexpr std::int64_t kChromeRows = 4;

/// ROWS OF THE RECORD A WHEEL NOTCH READS.
constexpr std::int64_t kWheelRows = 3;

// ---- Rendering one participant's record ---------------------------------------------
//
// EVERY FUNCTION HERE WAS `screen_terminal.cpp`'s, and every one of them is presentation:
// what a sentence says, and how many rows of THIS pane it costs. They read a `ShownEntry`
// instead of a `loom::TranscriptEntry`, which is the same ten facts.

std::string entry_address(const ShownEntry& e) {
    if (e.addressing == ws::kAddressWeave) {
        return "#" + std::to_string(e.target);
    }
    if (e.addressing == ws::kAddressRole) {
        return "@" + e.role;
    }
    if (e.addressing == ws::kAddressPublish) {
        return "* (" + std::to_string(e.recipients) + " queued)";
    }
    return "?";
}

std::string entry_shape(const ShownEntry& e) {
    return e.shape + " v" + std::to_string(e.version);
}

std::string entry_line(const ShownEntry& e) {
    if (e.kind == ws::kEntryCommand) {
        return "> " + e.text;
    }
    if (e.kind == ws::kEntryRefusal) {
        return "!! " + e.text;
    }
    if (e.kind == ws::kEntryNotice) {
        return "-- " + e.text;
    }
    if (e.kind == ws::kEntrySubmitted) {
        return "^ " + entry_shape(e) + " -> " + entry_address(e) + "  SUBMITTED";
    }
    if (e.kind == ws::kEntryReceived) {
        return "v " + entry_shape(e) + " from #" + std::to_string(e.sender);
    }
    if (e.kind == ws::kEntryAnswer) {
        return "v " + entry_shape(e) + " from #" + std::to_string(e.sender) +
               "  [Loom: answers ask " + std::to_string(e.answers) + "]";
    }
    return e.text;
}

/// WHAT `^` MEANS, said once, on a row the pane always shows.
std::string legend_text() { return "SUBMITTED = authored; a sender is not told its fate"; }

std::vector<std::string> entry_wrapped(const ShownEntry& e, std::int64_t width) {
    return wrap(entry_line(e), width);
}

/// THE RECORD AS ROWS OF ONE WIDTH: every entry wrapped, and the row each entry starts on. The
/// unit a view scrolls by and a marker counts in is a ROW, because a wrapped entry cut at the top
/// of a view is part of one entry and not one of a number of messages.
struct WrappedRecord {
    std::vector<std::string> rows;
    std::vector<std::int64_t> starts;
    std::vector<std::size_t> subjects;
    std::int64_t total() const { return static_cast<std::int64_t>(rows.size()); }
};

WrappedRecord wrap_record(const std::vector<ShownEntry>& entries, std::int64_t width) {
    WrappedRecord out;
    out.starts.reserve(entries.size());
    for (const ShownEntry& e : entries) {
        out.starts.push_back(out.total());
        for (std::string& one : entry_wrapped(e, width)) {
            out.rows.push_back(std::move(one));
            out.subjects.push_back(out.starts.size() - 1);
        }
    }
    return out;
}

/// WHAT IS ABOVE THE VIEW, said at its top: retained rows scrolled past, and -- a different fact --
/// entries the participant evicted for good, which no scroll reaches.
std::string omission_text(std::int64_t above, std::int64_t below, std::int64_t dropped, bool lost) {
    std::string text;
    if (lost) {
        text = "... what you were reading was dropped for good";
    } else if (above > 0) {
        text = "... " + std::to_string(above) + " more rows above";
    } else if (dropped == 0) {
        return below > 0 ? "[the start of this session's record]"
                         : "[the whole of this session's record is on screen]";
    } else {
        text = "[the oldest kept]";
    }
    if (dropped > 0 && !lost) {
        text += "; " + std::to_string(dropped) + " older entries dropped for good";
    }
    return text;
}

/// WHAT IS BELOW THE VIEW, said at its bottom, and the way back to the newest output.
std::string below_text(std::int64_t below) {
    return "... " + std::to_string(below) + " more rows below -- press here for the newest";
}

/// WHICH SLICE OF A LIST IS SHOWN, given the selection and the room: the window follows the
/// selection and never scrolls past the end.
std::size_t first_shown(std::size_t selected, std::size_t total, std::size_t room) {
    if (room == 0 || total <= room) {
        return 0;
    }
    if (selected < room) {
        return 0;
    }
    const std::size_t last_first = total - room;
    const std::size_t want = selected - room + 1;
    return want < last_first ? want : last_first;
}

// =============================================================================

class TerminalPaneWeave
    : public loom::WeaveBase<
          TerminalPaneWeave, pane::TerminalPaneState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed, ws::v3::PanePressed,
                       ws::PaneDragged, ws::PaneButton, ws::PaneMenuAnswered,
                       ws::TerminalValueAnswered, ws::PaneCarryAnswered, loom::DispatchRefused, PaneKey,
                       PaneTextInput, PaneWheel, PaneActionRequested, TranscriptShown,
                       TerminalActed, TerminalCompletionOffered, surface::ClipboardCopy,
                       surface::ClipboardText>,
          loom::Emit<PaneOffered, PaneActions, PaneContent, ws::v3::PaneContent, PaneCaret, PaneEscapeUnspent,
                     ws::TerminalValueRequested, ws::PaneValueCarryRequested, ws::PaneMenuRequested,
                     ws::PanePassRequested,
                     TerminalActRequested, TerminalCompletionRequested, surface::ClipboardCopy,
                     surface::ClipboardTextRequested>> {
public:
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        // THE LINE A RELOAD KEPT, PUT BACK -- with the caret at its end, which is where the
        // completer requires it and where a maker about to keep typing wants it.
        line_.set(state_.line, state_.line.size());
        announce(mail);
    }

    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        announce(mail);
    }

    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != pane::kTerminalPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        granted_ = true;
        say(mail);
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
        // at its top until the maker scrolls again. Nothing else in a new picture moves the view.
        if (!reading_.follow && reading_.seq < known_.dropped) {
            reading_.seq = known_.dropped;
            reading_.row = 0;
            reading_.lost = true;
        }
        say(mail);
    }

    /// THE WHEEL READS THE RECORD: three rows a notch, fractions carried, +1 away from the maker
    /// being older output. Reading moves no line, recall, list or notice.
    void on(const PaneWheel& wheel, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || wheel.pane != pane::kTerminalPane) {
            return;
        }
        wheel_ += wheel.dy * static_cast<double>(kWheelRows);
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
    /// the answer a maker reads. A refusal is the door's own words and belongs beside the
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

    /// What could be said next: the answer is the participant's, and which candidate the maker
    /// is on is this pane's, surviving a recomputation on its own rule. First, is it still about
    /// this line? Between ask and answer a maker can empty the line, move the caret off its end,
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
            selected_ = 0;
        }
        // A DISMISSAL BELONGS TO THE PART OF THE LINE IT WAS MADE IN. Moving on to the next
        // word is a new question, so the list comes back for it.
        if (dismissed_ && offered_.slot != dismissed_at_) {
            dismissed_ = false;
        }
        say(mail);
    }

    using Subject = std::pair<std::int64_t, std::int64_t>; // terminal instance, observation
    void on(const ws::v3::PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != pane::kTerminalPane) return;
        if (!subjects_.current(press.picture)) {
            notice_ = "That transcript picture changed; try again"; say(mail); return;
        }
        if (const auto* subject = subjects_.at(press.row, press.column)) {
            acquire(*subject, true, mail); return;
        }
        on(PanePressed{press.pane, press.row, press.column}, mail);
    }
    void on(const ws::PaneDragged&, loom::Mail&) {}
    void on(const ws::PaneButton& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != pane::kTerminalPane ||
            !press.pressed || press.button != 3 || press.lost || capture_.pending()) return;
        if (!subjects_.current(press.picture)) {
            notice_ = "That transcript picture changed; try again"; say(mail); return;
        }
        const auto* subject = subjects_.at(press.row, press.column);
        if (!subject) { ws::pane_menu::pass_back(mail, pane::kTerminalPaneRole, pane::kTerminalPane); return; }
        menu_subject_ = *subject;
        menu_ = ws::pane_menu::Offer(pane::kTerminalPane, "Transcript value")
                    .at(press.row, press.column).row("copy", "Pick up a copy")
                    .send(mail, pane::kTerminalPaneRole);
    }
    void on(const ws::PaneMenuAnswered& answer, loom::Mail& mail) {
        if (menu_.take(mail, answer) == "copy") acquire(menu_subject_, false, mail);
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
    /// TWO ROWS MEAN SOMETHING: a candidate row chooses that candidate, and the input row
    /// places the caret. Everything else is consumed and changes nothing, which is what a
    /// pane that owns visible room owes the cells it is not using.
    void on(const PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != pane::kTerminalPane) {
            return;
        }
        if (press.row == input_row_ && input_row_ >= 0) {
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
            const std::int64_t offset = press.column - kPromptCols;
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
        if (press.row == bottom_marker_row_ && bottom_marker_row_ >= 0) {
            reading_ = Reading{};
            say(mail);
            return;
        }
        if (list_first_row_ >= 0 && press.row >= list_first_row_ &&
            press.row < list_first_row_ + list_row_count_) {
            // ROW 0 OF THE LIST IS THE HEADING and is not a candidate. A press on it is a
            // press on the list -- consumed, changing nothing.
            const std::int64_t at = press.row - list_first_row_ - 1;
            if (at < 0) {
                return;
            }
            // ONE SELECTION, WHICHEVER HAND MOVED IT: this writes the field Up/Down write,
            // so the row a click chooses is a row the completion key then accepts.
            const std::size_t index = list_shown_first_ + static_cast<std::size_t>(at);
            if (index < offered_.candidates.size()) {
                selected_ = index;
                say(mail);
            }
        }
    }

    /// THE LINE'S OWN VOCABULARY: the editing keys, selection, clipboard and word movement,
    /// all through the one component call every consumer makes. A gesture this pane declared
    /// a row for never reaches here -- it arrives as `PaneActionRequested` instead.
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
            mail.publish(surface::ClipboardCopy{clip_.text});
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

    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || asked.pane != pane::kTerminalPane) {
            return;
        }
        // AN ID THIS PANE NEVER DECLARED IS NO ACT (`agents/panes.md`), so it spends nothing: not
        // the notice, and not the memory that lets a second press in a word select it. Every id
        // the pane does declare says its rows below, so what an act spends is always published.
        if (!answers(asked.id)) {
            return;
        }
        // THE READING KEYS MOVE THE VIEW AND NOTHING ELSE: not the line, a recall, a list, a
        // refusal beside the line or the memory that makes a second press select a word.
        if (asked.id == pane::kActionScrollUp || asked.id == pane::kActionScrollDown ||
            asked.id == pane::kActionOldest || asked.id == pane::kActionNewest) {
            if (asked.id == pane::kActionScrollUp) {
                scroll(-page());
            } else if (asked.id == pane::kActionScrollDown) {
                scroll(page());
            } else if (asked.id == pane::kActionOldest) {
                scroll(-wrap_record(known_.entries, columns_).total());
            } else {
                reading_ = Reading{};
            }
            say(mail);
            return;
        }
        word_press_ = false;
        notice_.clear();
        history_note_.clear();
        // (!) ENTER AND TAB ON A RECALLED LINE LOCK IT IN AND DO NOTHING ELSE. The press is spent
        // whole on ending the recall: no submission, no candidate taken, and no list asked for.
        // The next Enter or Tab has its ordinary meaning.
        if (asked.id == pane::kActionSubmit || asked.id == pane::kActionComplete) {
            if (settle_recall()) {
                remember_line();
                say(mail);
                return;
            }
        }
        if (asked.id == pane::kActionSubmit) {
            submit(mail);
            return;
        }
        // UP AND DOWN WALK THE HISTORY WHEN NO COMMAND IS BEING COMPOSED, and keep walking it
        // while a recalled line is browsed; on a command being composed they move the list.
        if (asked.id == pane::kActionUp || asked.id == pane::kActionDown) {
            const int by = asked.id == pane::kActionUp ? -1 : +1;
            if (recall_.active || composing_nothing()) {
                recall(by, mail);
                return;
            }
            move_selection(by);
            say(mail);
            return;
        }
        if (asked.id == pane::kActionComplete) {
            // ONE KEY, ONE MEANING: "help me here". With a list on screen that is taking the
            // selected candidate; with nothing on screen it is asking for one, which is the
            // only gesture discovery needs because every other entry point is typing.
            if (selectable()) {
                accept_candidate();
                moved();
                remember_line();
                ask_completion(mail);
            } else {
                asked_for_list_ = true;
                dismissed_ = false;
                moved();
                ask_completion(mail);
            }
            say(mail);
            return;
        }
        if (asked.id == pane::kActionBack) {
            if (recall_.active) {
                // ESCAPE ON A RECALLED LINE GOES BACK TO THE LINE BEFORE THE RECALL, whole.
                cancel_recall();
            } else if (selectable()) {
                // THE LIST GOES AWAY AND THE LINE IS UNTOUCHED. A maker who wanted the line
                // gone presses it again; a maker who wanted only the list gone has not lost
                // the word they were half-way through.
                dismissed_ = true;
                dismissed_at_ = offered_.slot;
                asked_for_list_ = false;
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

    void on(const surface::ClipboardCopy& said, loom::Mail&) {
        // WHAT THE PROCESS SAYS IT COPIED -- this pane's own copies included, which is why
        // it is a mirror rather than a second store: a paste answers with this.
        clip_.text = said.text;
    }

    /// The Skin's answer to a paste this pane asked for: the one road foreign clipboard text has
    /// into this line, under a maker's own gesture. The correlation says which ask, not that the
    /// line that asked still exists: text asked for by a draft that has ended lands nowhere
    /// (WL-TEXT-09). It goes in through `TextBox::paste` -- one undo entry, `pasteable_line`
    /// flattening tabs and line breaks -- and the gate judges the text that would land: what is
    /// still undrawable is refused whole, aloud, on the notice row.
    void on(const surface::ClipboardText& a, loom::Mail& mail) {
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

    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kTerminalPaneRole)
            .send_to_role(kWorkshopRole,
                          PaneOffered{pane::kTerminalPane, pane::kTerminalPaneName,
                                      pane::kTerminalPaneSummary, 10, 72});
        declare(mail);
    }

    /// What this pane answers to: nine rows, and they never change. No modes, unlike Info's and
    /// Files': the line is always open (it is the pane), so each id names one gesture whose
    /// meaning the pane resolves against its state (a recall, a list, a line), and every other
    /// key reaches the line as a `PaneKey`.
    void declare(loom::Mail& mail) {
        PaneActions actions;
        actions.pane = pane::kTerminalPane;
        actions.rows = action_rows();
        (void)mail.as_role(pane::kTerminalPaneRole).send_to_role(kWorkshopRole, actions);
    }

    /// THE ROWS -- what `declare` tells Workshop, and what `answers` reads, so what the pane
    /// acts on and what it said it acts on are one list.
    static std::vector<PaneActionRow> action_rows() {
        std::vector<PaneActionRow> rows;
        const auto row = [&rows](const char* id, const char* label, std::int64_t sc) {
            rows.push_back(PaneActionRow{id, label, sc, input::mod::kNone});
        };
        row(pane::kActionSubmit, "run the line / keep a recall", input::scan::kReturn);
        row(pane::kActionComplete, "what can this terminal say?", input::scan::kTab);
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

    static bool answers(const std::string& id) {
        for (const PaneActionRow& row : action_rows()) {
            if (row.id == id) {
                return true;
            }
        }
        return false;
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
        selected_ = 0;
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

    /// Ask what could be said next: at most one question outstanding, never from a place the
    /// answer could not be about. Every exit leaves the offer applying to the line that is there:
    /// the three silences below re-stamp `offered_about_`, and the ask stamps `asked_about_`, so
    /// the answer can be measured against the line it comes back to.
    void ask_completion(loom::Mail& mail) {
        wanted_ = true;
        wanted_intent_ = intent_;
        if (recall_.active) {
            // A RECALLED LINE IS BROWSED, NOT COMPOSED: no list is asked for it until the maker
            // locks it in or edits it.
            offered_ = TerminalCompletionOffered{};
            offered_about_ = here();
            offered_intent_ = intent_;
            return;
        }
        if (!known_.attached) {
            offered_ = TerminalCompletionOffered{};
            offered_about_ = here();
            offered_intent_ = intent_;
            return; // nothing to ask, and a door that would answer "nothing" anyway
        }
        // Asked about the end of the line, where the caret must be: accepting a candidate drops
        // the typed part of the last token and appends the rest, which with a mid-line caret
        // would delete everything after it. So the pane says so aloud rather than going quiet,
        // since three different silences would look identical.
        if (!line_.at_end()) {
            offered_ = TerminalCompletionOffered{};
            offered_.open = true;
            offered_.heading = "completion follows the END of the line -- this caret is inside it";
            offered_about_ = here();
            offered_intent_ = intent_;
            selected_ = 0;
            return;
        }
        // AN UNTOUCHED LINE ASKS NOTHING. The line is empty immediately after a submit, and a
        // list there covers the answer the pane just gave. Typing is the gesture; the
        // completion key is the way to ask anyway.
        if (line_.empty() && !asked_for_list_) {
            offered_ = TerminalCompletionOffered{};
            offered_about_ = here();
            offered_intent_ = intent_;
            selected_ = 0;
            return;
        }
        if (completing_) {
            return; // one question at a time; its answer is dropped and asked again (`wanted_`)
        }
        asked_about_ = here();
        asked_intent_ = intent_;
        completion_pending_ = ++asked_;
        completing_ = true;
        (void)mail.as_role(pane::kTerminalPaneRole)
            .send_to_role(kWorkshopRole, TerminalCompletionRequested{line_.text()},
                          completion_pending_);
    }

    /// ...and the draft that asked, so the answer is measured against the draft it comes back
    /// to (WL-TEXT-09): `set` and `clear` end a draft and bump this counter, so an edit is the
    /// same draft and an abandoned or submitted line is not.
    void begin_paste(loom::Mail& mail) {
        paste_.pending = ++asked_;
        paste_.epoch = line_.draft_epoch();
        paste_.awaiting = true;
        (void)mail.as_role(pane::kTerminalPaneRole)
            .send_to_role(zengine::surface::kSkinRole, surface::ClipboardTextRequested{},
                          paste_.pending);
    }

    // ---- The completion list's own cursor -------------------------------------------------

    /// WHETHER THERE IS A CANDIDATE TO ACT ON -- and the first half of the question is
    /// whether what this pane is holding is about the line in front of the maker at all
    /// (`offer_applies`). A list that outlived its line is not a list.
    bool selectable() const {
        return offer_applies() && offered_.open && !dismissed_ && !offered_.candidates.empty();
    }

    void move_selection(int by) {
        if (!selectable()) {
            return;
        }
        const std::size_t last = offered_.candidates.size() - 1;
        if (by < 0) {
            selected_ = selected_ == 0 ? 0 : selected_ - 1;
        } else {
            selected_ = selected_ >= last ? last : selected_ + 1;
        }
    }

    void accept_candidate() {
        if (!selectable()) {
            return;
        }
        const ShownCandidate& c = offered_.candidates[selected_];
        // `partial` IS A TOKEN OF THIS VERY LINE, so it can never be longer than the line.
        // The `min` is written anyway rather than as an `if`, because the alternative to
        // clamping is appending without stripping, which is a doubled word on a line nobody
        // could explain.
        const std::size_t typed =
            offered_.partial.size() < line_.size() ? offered_.partial.size() : line_.size();
        std::string text = line_.text();
        text.resize(text.size() - typed);
        text += c.insert;
        const std::size_t at = text.size();
        line_.set(std::move(text), at);
    }

    /// THE ONE PLACE THE KEPT LINE IS WRITTEN. `state_` is what a same-shape reload carries,
    /// so it is updated wherever the text changes and nowhere else. A recalled line is browsed
    /// and not yet adopted, so while one is on the line the kept line is the draft before it.
    void remember_line() { state_.line = recall_.active ? recall_.before.text() : line_.text(); }

    // ---- Reading the record: the view's top, and the gestures that move it -------------------

    /// THE ROW AT THE TOP OF A VIEW `view` ROWS TALL. Following, the row that puts the newest row
    /// at the bottom; anchored, the anchor's row -- clamped into the entry a re-wrap may have
    /// shortened, and never so far down that the view runs past the newest row.
    std::int64_t view_top(const WrappedRecord& record, std::int64_t view) const {
        const std::int64_t last = record.total() > view ? record.total() - view : 0;
        if (reading_.follow) {
            return last;
        }
        const std::int64_t index = reading_.seq - known_.dropped;
        if (index < 0 || record.starts.empty()) {
            return 0;
        }
        if (index >= static_cast<std::int64_t>(record.starts.size())) {
            return last;
        }
        const std::size_t i = static_cast<std::size_t>(index);
        const std::int64_t end =
            i + 1 < record.starts.size() ? record.starts[i + 1] : record.total();
        const std::int64_t cost = end - record.starts[i];
        const std::int64_t row = reading_.row < cost ? reading_.row : (cost > 0 ? cost - 1 : 0);
        const std::int64_t top = record.starts[i] + row;
        return top < last ? top : last;
    }

    /// MOVE THE VIEW BY `by` ROWS (negative is older). Reaching the newest row follows it again;
    /// anywhere else the view is anchored to the entry and row now at its top.
    void scroll(std::int64_t by) {
        const WrappedRecord record = wrap_record(known_.entries, columns_);
        const std::int64_t view = view_rows_ > 0 ? view_rows_ : 1;
        const std::int64_t last = record.total() > view ? record.total() - view : 0;
        std::int64_t top = view_top(record, view) + by;
        top = top < 0 ? 0 : top;
        reading_.lost = false;
        if (top >= last) {
            reading_ = Reading{};
            return;
        }
        std::size_t i = 0;
        while (i + 1 < record.starts.size() && record.starts[i + 1] <= top) {
            ++i;
        }
        reading_.follow = false;
        reading_.seq = known_.dropped + static_cast<std::int64_t>(i);
        reading_.row = top - (record.starts.empty() ? 0 : record.starts[i]);
    }

    /// A PAGE OF THE VIEW, keeping one row of the page before it in sight.
    std::int64_t page() const { return view_rows_ > 1 ? view_rows_ - 1 : 1; }

    // ---- Command history: the participant's own record, walked ------------------------------

    /// ONE COMMAND A MAKER CAN RECALL, named by its place in the record's whole history.
    struct Recallable {
        std::int64_t seq = 0;
        std::string text;
    };

    /// THE COMMANDS THE RECORD HOLDS, NEWEST FIRST. Only `command` entries -- what a
    /// presentation asked this participant to run, recorded before it was parsed -- and never
    /// an answer, a notice or a refusal. A run of the same command typed again and again is
    /// walked as one, as its newest. Bounded by the record's own owner, so a command the
    /// participant evicted is no longer here, and nothing in this pane keeps a copy of one.
    std::vector<Recallable> recallable() const {
        std::vector<Recallable> out;
        for (std::size_t i = known_.entries.size(); i > 0; --i) {
            const ShownEntry& e = known_.entries[i - 1];
            if (e.kind != ws::kEntryCommand || e.text.empty()) {
                continue;
            }
            if (!out.empty() && out.back().text == e.text) {
                continue;
            }
            out.push_back(Recallable{known_.dropped + static_cast<std::int64_t>(i - 1), e.text});
        }
        return out;
    }

    /// NO COMMAND IS BEING COMPOSED: the line is empty and the maker has not asked for a list
    /// on it. This, or a recall already under way, is when Up and Down mean history.
    bool composing_nothing() const { return line_.empty() && !asked_for_list_; }

    /// ONE STEP THROUGH HISTORY: -1 older, +1 newer.
    void recall(int by, loom::Mail& mail) {
        const std::vector<Recallable> commands = recallable();
        if (!recall_.active) {
            if (commands.empty()) {
                history_note_ = "no command to recall yet";
            } else if (by > 0) {
                history_note_ = "nothing newer -- Up recalls the last command";
            } else {
                recall_.before = line_;
                show_recalled(commands.front());
            }
            say(mail);
            return;
        }
        const std::string now = line_.text();
        if (by < 0) {
            for (const Recallable& c : commands) {
                if (c.seq < recall_.seq && c.text != now) {
                    show_recalled(c);
                    say(mail);
                    return;
                }
            }
            history_note_ = known_.dropped > 0 ? "the oldest kept; " +
                                                     std::to_string(known_.dropped) +
                                                     " older entries dropped for good"
                                               : "the oldest command";
            say(mail);
            return;
        }
        for (std::size_t i = commands.size(); i > 0; --i) {
            const Recallable& c = commands[i - 1];
            if (c.seq > recall_.seq && c.text != now) {
                show_recalled(c);
                say(mail);
                return;
            }
        }
        // PAST THE NEWEST IS THE LINE BEFORE THE FIRST RECALL, exactly as it was left.
        cancel_recall();
        say(mail);
    }

    void show_recalled(const Recallable& c) {
        line_.set(c.text, c.text.size());
        recall_.active = true;
        recall_.seq = c.seq;
        dismissed_ = false;
        asked_for_list_ = false;
        selected_ = 0;
        moved();
        offered_ = TerminalCompletionOffered{};
        offered_about_ = here();
        offered_intent_ = intent_;
    }

    /// THE RECALLED LINE BECOMES THE DRAFT, where it stands, and the line before the recall is
    /// let go. Answers whether there was a recall to end.
    bool settle_recall() {
        if (!recall_.active) {
            return false;
        }
        recall_ = Recall{};
        moved();
        return true;
    }

    /// BACK TO THE DRAFT BEFORE THE FIRST RECALL -- the whole box, so its undo, its caret and
    /// the draft a paste in flight belongs to come back with its text.
    void cancel_recall() {
        line_ = recall_.before;
        recall_ = Recall{};
        moved();
        remember_line();
    }

    /// WHAT THE ROW ABOVE THE LINE SAYS ABOUT HISTORY, or nothing.
    std::string history_heading() const {
        if (!recall_.active) {
            return history_note_;
        }
        const std::vector<Recallable> commands = recallable();
        std::size_t newer = 0;
        for (const Recallable& c : commands) {
            newer += c.seq > recall_.seq ? 1 : 0;
        }
        std::string text = "history " + std::to_string(newer + 1) + " of " +
                           std::to_string(commands.size() > newer ? commands.size() : newer + 1);
        text += history_note_.empty() ? " -- Enter/Tab: edit, Esc: back" : " -- " + history_note_;
        return text;
    }

    /// THE LINE A QUESTION ABOUT COMPLETION IS ABOUT. Two of these are kept: what the
    /// OUTSTANDING ask was about (`asked_about_`), and what the offer in hand is about
    /// (`offered_about_`).
    struct Asking {
        std::string line;
        std::size_t caret = 0;
        bool operator==(const Asking& o) const { return caret == o.caret && line == o.line; }
        bool operator!=(const Asking& o) const { return !(*this == o); }
    };

    /// What a completion is about: the line, and where in it the maker stands. The text, because
    /// accepting strips `partial` off the end and appends `insert`, meaningless against a line
    /// the partial is not a token of; the caret, because completion follows the end of the line,
    /// and a mid-line caret would lose everything after it.
    Asking here() const { return Asking{line_.text(), line_.caret()}; }

    /// Is what this pane holds about the line in front of the maker? Asked before the list is
    /// drawn as well as before a candidate is taken. The cost is a publication with no list while
    /// a fresh answer is in flight (the completion is a round trip); whether a medium draws both
    /// canvases of that turn is the medium's business, unmeasured. What it buys: no candidate is
    /// ever offered against a line that is not on the screen.
    bool offer_applies() const { return offered_intent_ == intent_ && offered_about_ == here(); }

    /// THE MAKER'S ACT CHANGED WHAT A COMPLETION WOULD BE ABOUT: a new intent, which no answer
    /// already in flight belongs to, and no wish for a list until an act asks for one.
    void moved() {
        ++intent_;
        wanted_ = false;
    }

    // ---- The rows, and the caret beside them ----------------------------------------------

    /// The pane, composed: refusal, header, legend, what is above the view, the view, what is
    /// below it, the completion list or the history row, the input row, and the caret beside
    /// them. The budget is spent in priority order: the input row first (a Terminal with no line
    /// is not one), then a standing refusal, the header, the marker above the view and the
    /// legend; the rest is split between the list (at most half) and the transcript. Every row is
    /// budgeted before it is composed, so a late row can never take back the input line.
    void say(loom::Mail& mail) {
        if (!granted_) {
            return; // no room has been sent: there is nothing this pane could truthfully fill
        }
        subjects_.begin();
        input_row_ = -1;
        list_first_row_ = -1;
        list_row_count_ = 0;
        list_shown_first_ = 0;
        std::vector<surface::SurfaceTextRow> out;
        const auto push = [&out, this](std::string text, std::int64_t role) {
            out.push_back(surface::SurfaceTextRow{drawable(fit(std::move(text), columns_)), role});
        };
        if (rows_ <= 0 || columns_ <= 0) {
            (void)mail.as_role(pane::kTerminalPaneRole)
                .send_to_role(kWorkshopRole, ws::v3::PaneContent{pane::kTerminalPane, std::move(out), 0, subjects_.settle()});
            say_caret(mail);
            return;
        }
        // The input row is taken first and a notice second, before anything else is composed: a
        // refusal belongs beside the line it is about, inside the budget. Two rows is the smallest
        // room that holds both; in one row the line keeps it and the refusal is not shown.
        const bool notice = !notice_.empty() && rows_ >= 2;
        const std::int64_t body = rows_ - 1 - (notice ? 1 : 0);
        const bool header = body >= 1;
        const bool omission = body >= 2;
        const bool legend = body >= kChromeRows;
        std::int64_t rest = body - (header ? 1 : 0) - (omission ? 1 : 0) - (legend ? 1 : 0);
        if (rest < 0) {
            rest = 0;
        }

        // A REFUSAL THE DOOR GAVE, over the top row, where a maker will see it.
        if (notice) {
            push(notice_, surface::role::kAlert);
        }

        // THE HEADER NAMES THE IDENTITY whose record this is. A presentation may hold
        // controls for more than one identity, and the moment it stops saying which one it
        // is showing is the moment the two look like one thing with two windows.
        if (header) {
            push(known_.attached
                     ? "TERMINAL -- weave #" + std::to_string(known_.participant)
                     : "TERMINAL -- no participant was mounted on this bus",
                 surface::role::kAccent);
        }
        if (legend) {
            push(legend_text(), surface::role::kMuted);
        }

        // THE LIST'S SHARE, DECIDED BEFORE THE TRANSCRIPT'S so the transcript gets what is
        // left rather than the other way round.
        const bool list_open = offer_applies() && offered_.open && !dismissed_;
        // THE ROW ABOVE THE LINE SAYS WHICH COMMAND IS RECALLED when there is no list: browsing
        // history is a different state from composing, and a maker has to be able to see which.
        const std::string history = list_open ? std::string() : history_heading();
        std::size_t list_wanted = list_open ? offered_.candidates.size() + 1 /*the heading*/
                                            : (history.empty() ? 0 : 1);
        const std::size_t list_ceiling = static_cast<std::size_t>(rest / 2);
        if (list_wanted > list_ceiling) {
            list_wanted = list_ceiling;
        }
        // A LIST OF ONE ROW IS A HEADING WITH NO CANDIDATES UNDER IT, and that is a complete
        // answer rather than an empty box -- `kCompletionMinRows` was one for exactly this
        // reason, measured: with a floor of two, `send * s` showed nothing at all.
        if ((list_open || !history.empty()) && list_wanted == 0 && rest >= 2) {
            list_wanted = 1;
        }
        const std::int64_t transcript_rows = rest - static_cast<std::int64_t>(list_wanted);

        // THE TRANSCRIPT, WRAPPED AND SCROLLED -- one entry becomes as many rows as its sentence
        // needs, and the view is a window onto all of them. Following, the newest row is at the
        // bottom. Scrolled away, a row below the view says how much is below and is the press back
        // to the newest; where there is no row for it, the top marker says it instead.
        const WrappedRecord record = wrap_record(known_.entries, columns_);
        std::int64_t view = transcript_rows > 0 ? transcript_rows : 0;
        std::int64_t top = view_top(record, view);
        std::int64_t below = record.total() - (top + view);
        const bool bottom = !reading_.follow && below > 0 && view >= 2;
        if (bottom) {
            view -= 1;
            top = view_top(record, view);
            below = record.total() - (top + view);
        }
        below = below > 0 ? below : 0;
        view_rows_ = view;
        top_marker_row_ = -1;
        bottom_marker_row_ = -1;
        if (omission) {
            top_marker_row_ = static_cast<std::int64_t>(out.size());
            std::string said = omission_text(top, below, known_.dropped, reading_.lost);
            if (below > 0 && !bottom) {
                said += " -- " + std::to_string(below) + " more rows below";
            }
            push(std::move(said), surface::role::kMuted);
        }
        for (std::int64_t i = 0; i < view; ++i) {
            const std::int64_t at = top + i;
            if (at < record.total()) {
                const auto& entry = known_.entries[record.subjects[static_cast<std::size_t>(at)]];
                if (entry.observation > 0 && (entry.kind == ws::kEntrySubmitted ||
                    entry.kind == ws::kEntryReceived || entry.kind == ws::kEntryAnswer)) {
                    subjects_.row(static_cast<std::int64_t>(out.size()), {known_.participant, entry.observation});
                }
            }
            push(at < record.total() ? record.rows[static_cast<std::size_t>(at)] : std::string(),
                 surface::role::kFill);
        }
        if (bottom) {
            bottom_marker_row_ = static_cast<std::int64_t>(out.size());
            push(below_text(below), surface::role::kMuted);
        }
        if (list_wanted > 0 && list_open) {
            list_first_row_ = static_cast<std::int64_t>(out.size());
            list_row_count_ = static_cast<std::int64_t>(list_wanted);
            say_list(list_wanted, push);
        } else if (list_wanted > 0) {
            push(history, surface::role::kMuted);
        }

        // THE LINE BEING TYPED, AND -- while there is nothing on it -- the gesture that
        // answers "what can I say here". It is on this row rather than in the legend because
        // it is about what to do NEXT rather than about what a word means, and because it
        // erases itself: the moment a maker types anything the line has their text on it.
        const std::int64_t visible = columns_ - kPromptCols - kCaretCols;
        line_.keep_caret_visible(visible > 0 ? visible : 0);
        input_row_ = static_cast<std::int64_t>(out.size());
        const bool prompting = line_.empty() && !list_open;
        push(prompting ? std::string(">    Tab: what can this terminal say?  Up: recall a command")
                       : "> " + line_.visible(visible > 0 ? visible : 0),
             known_.attached ? surface::role::kAccent : surface::role::kAlert);

        if (static_cast<std::int64_t>(out.size()) > rows_) {
            out.resize(static_cast<std::size_t>(rows_));
        }
        if (input_row_ >= static_cast<std::int64_t>(out.size())) {
            input_row_ = -1; // cut away: there is no row to put a caret on
        }
        (void)mail.as_role(pane::kTerminalPaneRole)
            .send_to_role(kWorkshopRole, ws::v3::PaneContent{pane::kTerminalPane, std::move(out), 0, subjects_.settle()});
        say_caret(mail);
    }

    template <typename Push> void say_list(std::size_t room, const Push& push) {
        const std::size_t body = room - 1; // the heading always costs one
        const std::size_t first = first_shown(selected_, offered_.candidates.size(), body);
        list_shown_first_ = first;
        const std::size_t last = offered_.candidates.size() < first + body
                                     ? offered_.candidates.size()
                                     : first + body;
        std::string heading = offered_.heading;
        if (offered_.candidates.size() > body) {
            // WHICH SLICE, SAID OUT LOUD -- including the slice that is nothing at all, which
            // is what a pane too short for a single candidate row shows. "none of 5" is a
            // worse picture than five rows and a far better sentence than five rows' worth of
            // silence.
            heading = (body == 0 ? std::string("none")
                                 : std::to_string(first + 1) + "-" + std::to_string(last)) +
                      " of " + std::to_string(offered_.candidates.size()) + "  " + heading;
        }
        push(std::move(heading), surface::role::kMuted);
        for (std::size_t i = first; i < last; ++i) {
            const ShownCandidate& c = offered_.candidates[i];
            const bool chosen = i == selected_;
            std::string text = (chosen ? "> " : "  ") + c.display;
            if (!c.detail.empty()) {
                // The detail is what a candidate MEANS, and it is the first thing a narrow
                // pane gives up: `fit` cuts the whole row, so a list in a small window shows
                // names and a list in a large one shows names and meanings.
                text += "   " + c.detail;
            }
            push(std::move(text), chosen ? surface::role::kAccent : surface::role::kFill);
        }
    }

    /// WHERE THE CARET IS, AND WHAT IS SELECTED -- beside the rows, never inside them.
    ///
    /// ONE MEASURER: the column comes from the same window the row was written against and
    /// the same one a press is answered with, so a caret cannot land where the text is not
    /// and a click cannot land where the caret would not.
    void say_caret(loom::Mail& mail) {
        PaneCaret caret;
        caret.pane = pane::kTerminalPane;
        if (input_row_ < 0) {
            // NO CARET, said as a sentence rather than by silence: a pane cut down to
            // nothing still has to un-say the caret it published when it was taller.
            (void)mail.as_role(pane::kTerminalPaneRole).send_to_role(kWorkshopRole, caret);
            return;
        }
        const std::int64_t visible = columns_ - kPromptCols - kCaretCols;
        caret.row = input_row_;
        caret.column = kPromptCols + static_cast<std::int64_t>(line_.caret_column());
        // AND THE SELECTION, THE SAME WAY: the visible part of the component's own range,
        // prompt-shifted by the same arithmetic the caret goes through. A selection scrolled
        // wholly off the slice publishes nothing, which is the truthful picture of a row that
        // shows none of it.
        const component::TextBox::VisibleSpan span =
            line_.visible_selection(visible > 0 ? visible : 0);
        if (span.present()) {
            caret.sel_begin_row = input_row_;
            caret.sel_begin_col = kPromptCols + span.begin;
            caret.sel_end_row = input_row_;
            caret.sel_end_col = kPromptCols + span.end;
        }
        (void)mail.as_role(pane::kTerminalPaneRole).send_to_role(kWorkshopRole, caret);
    }

    // ---- State not in the shape --------------------------------------------------------

    zengine::ActivationCursor activation_;

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
    /// while browsing, or why Up or Down did nothing. Cleared by the maker's next act.
    std::string history_note_;

    /// WHERE THE MAKER IS READING THE RECORD. Following the newest output unless they scrolled
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

    /// WHAT THE PARTICIPANT SAID COULD COME NEXT, and which of it the maker is standing on.
    TerminalCompletionOffered offered_;
    Asking offered_about_;
    std::uint64_t offered_intent_ = 0;
    std::size_t selected_ = 0;
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

    /// WHICH COMPOSED ROW IS THE INPUT LINE, and where the list is -- written by `say` and
    /// read by a press, so a press and a picture cannot disagree about what is where.
    std::int64_t input_row_ = -1;
    std::int64_t list_first_row_ = -1;
    std::int64_t list_row_count_ = 0;
    std::size_t list_shown_first_ = 0;

    /// A PRESS THAT LANDED WHERE THE CARET ALREADY WAS, so the next one selects the word.
    bool word_press_ = false;
    std::size_t word_press_at_ = 0;

    /// ONE COUNTER FOR EVERY QUESTION THIS PANE ASKS, so a correlation is this incarnation's
    /// own and an answer to somebody else's question is not mistaken for one to ours.
    component::RowMap<Subject> subjects_;
    ws::pane_menu::Asked menu_;
    Subject menu_subject_{};
    loom::RoleRequest capture_;
    std::uint64_t capture_gesture_ = 0;
    bool capture_drag_ = false;
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

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(TerminalPaneWeave)
