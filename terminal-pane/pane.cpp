// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Terminal pane -- a loadable weave that offers Workshop one pane: the RECORD of the
// terminal participant this host mounted, and the LINE a maker composes messages on.
//
// IT USED TO BE AN OVERLAY INSIDE THE HOST (`Session::terminal`, `screen_terminal.cpp`'s
// `paint_terminal` and its twelve helpers, `weave_terminal.cpp`'s seven mode functions,
// `KeyContext::kTerminal`, six `Act` values, a global chord, a contextual row, four screen
// constants, six `Screen` fields, and a paint plane after every pane). Now it is a weave
// beside the Skin, the Timer, the browser, the Builder, Attention and Info.
//
// ⚠ THE PARTICIPANT DID NOT COME WITH IT, AND THAT IS MEASURED RATHER THAN PREFERRED.
// `loom::TerminalSession` is not driven by any message in the interface it has TODAY -- its
// handler sends nothing, by construction, and the only shapes it accepts are the three answer
// doors its host declared -- and this work adds no Loom sentence, so under that constraint it
// could not have moved. That is the bound, and it is narrower than "could not, ever": a Loom
// that gave the session a driven door would change the answer, and this is not that phase.
// So it stays where its trust lives: a narrow, host-mounted identity whose grant is one
// rule. What crosses is a PICTURE the host derives (`TranscriptShown`), one ACT
// (`TerminalActRequested`), and one READ (`TerminalCompletionRequested`). Nothing in this
// image can speak as that participant; it can ask it to speak, and be told what happened.
//
// ⚠ AND THE CARET CAME ACROSS, WHICH IS THIS MIGRATION'S ONE NEW SENTENCE. `PaneCaret` is
// published beside the rows by a pane that has a caret, and merged by Workshop into the
// region it assembles -- so the line a maker is typing has an insertion point and a
// selection again, on SDL as the bar between glyphs. The Editor's migration reuses it
// unchanged; the four panes that have no caret say nothing and pay nothing.
//
// ⚠ WHAT THE MOVE COSTS IS THE SELECTION DRAG. The built-in swept a selection across its
// line by re-resolving every pointer motion against the row (`text_drag_place::
// kTerminalLine`). A pane is sent a PRESS and is sent no motion and no release, so a sweep
// is not a gesture this seam has: a press places the caret, a second press in the same word
// selects it, and shift with the caret keys sweeps by keyboard. Naming it rather than
// working around it, because the route around it is a motion shape, and that is a protocol
// sentence this migration is not owed.

#include "terminal-pane/vocabulary.hpp"

#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"

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
using ws::PaneKey;
using ws::PaneOffered;
using ws::PanePressed;
using ws::PaneRoom;
using ws::PaneTextInput;
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

/// THE PROMPT, in columns: the `> ` before the editable text. The built-in's constant, and
/// it is a fact about a ROW this pane composes rather than about any screen.
constexpr std::int64_t kPromptCols = 2;

/// ONE COLUMN OF THE INPUT ROW THE TEXT MAY NOT USE -- the caret's own. A caret at the end
/// of a full row would otherwise sit one past the room, and in a cell projection the mark is
/// a character that needs a cell of its own.
constexpr std::int64_t kCaretCols = 1;

/// The chrome a pane spends on being this pane, whatever is in it: the header, the standing
/// legend, the omission marker and the input row. The built-in's `kTerminalChrome`, carried.
constexpr std::int64_t kChromeRows = 4;

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

/// HOW MANY OF THE NEWEST ENTRIES A PANE THIS WIDE AND THIS TALL CAN SHOW WHOLE.
///
/// A ROW APIECE IS THE FLOOR, so the first entry is always taken even when it is taller than
/// the pane -- an empty box is indistinguishable from a broken tool, which is why this
/// function counts one before it starts refusing.
std::size_t entries_that_fit(const std::vector<ShownEntry>& entries, std::int64_t width,
                             std::size_t rows) {
    std::size_t taken = 0;
    std::size_t used = 0;
    for (std::size_t i = entries.size(); i > 0; --i) {
        const std::size_t cost = entry_wrapped(entries[i - 1], width).size();
        if (taken > 0 && used + cost > rows) {
            break;
        }
        used += cost;
        ++taken;
        if (used >= rows) {
            break;
        }
    }
    return taken;
}

/// WHAT THE PANE IS NOT SHOWING, in two numbers that are two different facts: entries the
/// participant still holds above the top of this pane, and entries it has evicted for good.
std::string omission_text(std::int64_t earlier, std::int64_t dropped) {
    if (earlier == 0 && dropped == 0) {
        return "[the whole of this session's record is on screen]";
    }
    std::string text = "... " + std::to_string(earlier) + " earlier";
    if (dropped > 0) {
        text += ", " + std::to_string(dropped) + " dropped for good";
    }
    return text;
}

/// WHICH SLICE OF A LIST IS SHOWN, given the selection and the room. The built-in's
/// `completion_first_shown`, unchanged: the window follows the selection and never scrolls
/// past the end.
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
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed, PaneKey,
                       PaneTextInput, PaneActionRequested, TranscriptShown, TerminalActed,
                       TerminalCompletionOffered, surface::ClipboardCopy,
                       surface::ClipboardText>,
          loom::Emit<PaneOffered, PaneActions, PaneContent, PaneCaret, TerminalActRequested,
                     TerminalCompletionRequested, surface::ClipboardCopy,
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

    /// WHAT COULD BE SAID NEXT. The answer is the participant's; which candidate the maker
    /// is standing on is this pane's, and survives a recomputation on its own rule.
    ///
    /// ⚠ AND FIRST: IS IT STILL ABOUT THIS LINE? The correlation says which question this
    /// answers; it does not say the question still stands. Between the ask and the answer a
    /// maker can empty the line, move the caret off its end, submit, or type -- and an
    /// answer about the line as it WAS is a list of candidates for a word nobody is typing.
    /// Accepting one splices a stripped `partial` that is no longer on the line.
    void on(const TerminalCompletionOffered& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !completing_ || mail.correlation() != completion_pending_) {
            return;
        }
        completing_ = false;
        const bool moved_on = stale_;
        stale_ = false;
        if (asked_about_ != here()) {
            // THE LINE THIS WAS ABOUT IS GONE. The answer is dropped whole -- not shown, so
            // it cannot be read as being about this line, and not held, so it cannot be
            // accepted into one -- and the question is put again for the line that IS here,
            // which is the only party that can say whether there is one to ask.
            ask_completion(mail);
            say(mail);
            return;
        }
        // THE SELECTION SURVIVES A RECOMPUTATION AND NOT A CHANGE OF QUESTION. The question
        // is the SLOT and the PARTIAL together: same question, same selection; a different
        // word or a different part of the line is a new list and starts at the top. Clamped
        // either way, because a list can shrink under an unchanged partial.
        //
        // The built-in learned this the hard way -- the arrow keys appeared to do nothing at
        // all, because the recomputation that followed each move reset the selection -- and
        // it is the reason `selected` is not on the wire.
        const bool same_question = offered_.open && said.open && said.slot == offered_.slot &&
                                   said.partial == offered_.partial;
        offered_ = said;
        offered_about_ = asked_about_;
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
        // IF THE LINE MOVED WHILE THE ANSWER WAS IN FLIGHT, ASK AGAIN. At most one question
        // is outstanding at a time, so a maker typing faster than the drain coalesces into
        // one further ask rather than a queue of them. (The line moving is normally caught
        // above; this stands for the paths that move it without changing what `here()` says
        // -- a room grant between the two, say -- and costs one further ask when it fires.)
        if (moved_on) {
            ask_completion(mail);
        }
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
            const std::size_t was = line_.caret();
            const bool had_selection = line_.has_selection();
            // THROUGH THE WINDOW THE ROW WAS DRAWN WITH. A visible column names
            // `first_visible + offset` of the WHOLE authored line, never the offset alone --
            // that is the one subtraction a horizontal viewport adds to a hit test.
            const std::int64_t offset = press.column - kPromptCols;
            const std::size_t target =
                line_.position_at_column(offset < 0 ? 0 : offset);
            // ...AND A SECOND PRESS IN THE SAME WORD SELECTS IT, the built-in's own rule.
            // The first press still places the caret and still means what it always did.
            if (word_press_ && target == was && was == word_press_at_) {
                line_.select_word_at(target);
                word_press_ = false;
            } else {
                line_.place(target);
                word_press_ = true;
                word_press_at_ = line_.caret();
            }
            if (line_.caret() != was || had_selection || line_.has_selection()) {
                ask_completion(mail); // the caret moving changes whether completion may ask
            }
            say(mail);
            return;
        }
        word_press_ = false;
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
            return;
        }
        notice_.clear();
        if (clip_.writes != copied_before) {
            mail.publish(surface::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_paste(mail);
        }
        // AN EDIT OR A CARET MOVE CHANGES WHETHER THE COMPLETER MAY BE ASKED, and what it
        // would answer, so both reach the ask. The built-in fell through to `refresh_terminal`
        // here for exactly this reason.
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
        // AT THE CARET, WHICH IS NOT ALWAYS THE END. `type` is the only door that moves the
        // text and the caret together, so a keystroke in the middle of a line cannot leave
        // one behind. Typing IS the completion gesture.
        line_.type(typed.text);
        asked_for_list_ = false;
        remember_line();
        ask_completion(mail);
        say(mail);
    }

    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || asked.pane != pane::kTerminalPane) {
            return;
        }
        word_press_ = false;
        notice_.clear();
        if (asked.id == pane::kActionSubmit) {
            submit(mail);
            return;
        }
        if (asked.id == pane::kActionUp) {
            move_selection(-1);
            say(mail);
            return;
        }
        if (asked.id == pane::kActionDown) {
            move_selection(+1);
            say(mail);
            return;
        }
        if (asked.id == pane::kActionComplete) {
            // ONE KEY, ONE MEANING: "help me here". With a list on screen that is taking the
            // selected candidate; with nothing on screen it is asking for one, which is the
            // only gesture discovery needs because every other entry point is typing.
            if (selectable()) {
                accept_candidate();
                remember_line();
                ask_completion(mail);
            } else {
                asked_for_list_ = true;
                dismissed_ = false;
                ask_completion(mail);
            }
            say(mail);
            return;
        }
        if (asked.id == pane::kActionBack) {
            if (selectable()) {
                // THE LIST GOES AWAY AND THE LINE IS UNTOUCHED. A maker who wanted the line
                // gone presses it again; a maker who wanted only the list gone has not lost
                // the word they were half-way through.
                dismissed_ = true;
                dismissed_at_ = offered_.slot;
                asked_for_list_ = false;
            } else {
                // ABANDONING THE LINE ABANDONS THE DISMISSAL WITH IT. The dismissal was made
                // against a word; there is no longer a word, so keeping it would leave the
                // list hidden for the whole of the next command with nothing on screen to
                // explain why.
                line_.clear(); // ...and the caret with it: `clear` moves both
                dismissed_ = false;
                remember_line();
                ask_completion(mail);
            }
            say(mail);
        }
    }

    void on(const surface::ClipboardCopy& said, loom::Mail&) {
        // WHAT THE PROCESS SAYS IT COPIED -- this pane's own copies included, which is why
        // it is a mirror rather than a second store: a paste answers with this.
        clip_.text = said.text;
    }

    /// THE SKIN'S ANSWER TO A PASTE THIS PANE ASKED FOR (QR-11) -- the one road foreign
    /// clipboard text has into this line, walked only under a maker's own gesture.
    ///
    /// ⚠ TWO QUESTIONS, AND THE CORRELATION ANSWERS ONLY THE FIRST. It says this is the
    /// answer to an ask this incarnation made. It does not say the line that asked still
    /// exists -- and between the ask and the answer a maker can abandon the command whole
    /// and start a different one, which is `clear` and a new draft. Text asked for by a
    /// draft that has ended lands nowhere, which is the law every other box in this
    /// repository already keeps (`files/`, `introspection/`, `composer/`, WL-TEXT-09).
    ///
    /// ⚠ AND IT GOES IN THROUGH `TextBox::paste`, WHICH IS THE COMPONENT'S OWN DOOR FOR
    /// THIS. The migration reached for `type` instead and lost two things the built-in had,
    /// both invisible until a maker did the next thing:
    ///
    ///   the UNDO GROUP     `type` coalesces into the typing burst before it, so `keep`,
    ///                      paste `OLD`, Ctrl+Z took the whole line. `paste` is one
    ///                      gesture and one entry, like a cut.
    ///   the NORMALIZATION  `pasteable_line` turns a tab, an LF, a CR and a CRLF pair into
    ///                      one space apiece, which is what a SINGLE-LINE field can hold.
    ///                      Gating on `admissible` instead refused two copied lines whole,
    ///                      and said nothing about it.
    ///
    /// SO THE GATE ASKS ABOUT THE TEXT THAT WOULD LAND, not the text that arrived: normalize
    /// first, then judge. What survives that and is still undrawable is a byte outside
    /// printable ASCII, and THAT is refused whole -- this pane's own typed door refuses one
    /// for the same reason, and a line holding bytes its own row draws as spaces would show
    /// a maker something other than what they would submit. Refused ALOUD, on the notice
    /// row: the editor says so on its own line and a silent whole-refusal of a paste is the
    /// exact shape of failure this correction exists about.
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
                                      pane::kTerminalPaneSummary});
        declare(mail);
    }

    /// WHAT THIS PANE ANSWERS TO -- five rows, and they never change.
    ///
    /// ⚠ UNLIKE INFO'S AND FILES', THIS DECLARATION HAS NO MODES. Those panes re-declare
    /// because a draft has to take Return and Escape away from the rows they otherwise mean.
    /// Here the line is ALWAYS open -- it is the pane -- so Return always submits and Escape
    /// always means "back". Every other key reaches the line as an ordinary `PaneKey`, which
    /// is what lets Backspace delete a character rather than meaning anything of this pane's.
    void declare(loom::Mail& mail) {
        PaneActions actions;
        actions.pane = pane::kTerminalPane;
        const auto row = [&actions](const char* id, const char* label, std::int64_t sc) {
            actions.rows.push_back(PaneActionRow{id, label, sc, input::mod::kNone});
        };
        row(pane::kActionSubmit, "run the line", input::scan::kReturn);
        row(pane::kActionComplete, "what can this terminal say?", input::scan::kTab);
        row(pane::kActionUp, "completion up", input::scan::kUp);
        row(pane::kActionDown, "completion down", input::scan::kDown);
        row(pane::kActionBack, "dismiss list / clear line", input::scan::kEscape);
        (void)mail.as_role(pane::kTerminalPaneRole).send_to_role(kWorkshopRole, actions);
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
        remember_line();
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

    /// ASK WHAT COULD BE SAID NEXT -- at most one question outstanding, and never from a
    /// place the answer could not be about.
    ///
    /// ⚠ EVERY EXIT FROM HERE LEAVES THE OFFER APPLYING TO THE LINE THAT IS THERE. The three
    /// silences below are answers this pane composed about the line as it is now, so each one
    /// re-stamps `offered_about_`; the ask stamps `asked_about_` instead, so the answer that
    /// comes back can be measured against the line it comes back to.
    void ask_completion(loom::Mail& mail) {
        if (!known_.attached) {
            offered_ = TerminalCompletionOffered{};
            offered_about_ = here();
            return; // nothing to ask, and a door that would answer "nothing" anyway
        }
        // AND IT IS ASKED ABOUT THE END OF THE LINE, WHICH IS WHERE THE CARET HAS TO BE. The
        // completer rests on an assumption that was free while the caret could not move: the
        // token being completed is the LAST one, so accepting is "drop what has been typed of
        // this token, append what it was going to be". With a caret in the middle that edit
        // would delete everything after it. So the pane says so out loud rather than going
        // quiet -- three different silences would otherwise render identically, and a maker
        // who moves the caret and watches the list vanish cannot tell "not here" from
        // "broken".
        if (!line_.at_end()) {
            offered_ = TerminalCompletionOffered{};
            offered_.open = true;
            offered_.heading = "completion follows the END of the line -- this caret is inside it";
            offered_about_ = here();
            selected_ = 0;
            return;
        }
        // AN UNTOUCHED LINE ASKS NOTHING. The line is empty immediately after a submit, and a
        // list there covers the answer the pane just gave. Typing is the gesture; the
        // completion key is the way to ask anyway.
        if (line_.empty() && !asked_for_list_) {
            offered_ = TerminalCompletionOffered{};
            offered_about_ = here();
            selected_ = 0;
            return;
        }
        if (completing_) {
            stale_ = true; // one question at a time; the answer will re-ask
            return;
        }
        asked_about_ = here();
        completion_pending_ = ++asked_;
        completing_ = true;
        (void)mail.as_role(pane::kTerminalPaneRole)
            .send_to_role(kWorkshopRole, TerminalCompletionRequested{line_.text()},
                          completion_pending_);
    }

    /// ...AND THE DRAFT THAT ASKED, so the answer can be measured against the draft it comes
    /// back to (QR-11, WL-TEXT-09). `set` and `clear` are the two doors that end a draft and
    /// the two that bump this counter, so an EDIT is the same draft and an abandoned or
    /// submitted line is not.
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
    /// so it is updated wherever the text changes and nowhere else.
    void remember_line() { state_.line = line_.text(); }

    /// THE LINE A QUESTION ABOUT COMPLETION IS ABOUT. Two of these are kept: what the
    /// OUTSTANDING ask was about (`asked_about_`), and what the offer in hand is about
    /// (`offered_about_`).
    struct Asking {
        std::string line;
        std::size_t caret = 0;
        bool operator==(const Asking& o) const { return caret == o.caret && line == o.line; }
        bool operator!=(const Asking& o) const { return !(*this == o); }
    };

    /// WHAT A COMPLETION IS ABOUT: the line, and where in it the maker is standing.
    ///
    /// BOTH HALVES ARE LOAD-BEARING, and each is a defect on its own. The TEXT, because a
    /// candidate is accepted by stripping `partial` off the end of the line and appending
    /// `insert` -- an arithmetic that means nothing against a line the partial is not a
    /// token of. The CARET, because completion follows the END of the line: an answer asked
    /// for at the end and read with the caret inside it would splice at the end and delete
    /// everything the maker had moved back to look at.
    Asking here() const { return Asking{line_.text(), line_.caret()}; }

    /// IS WHAT THIS PANE IS HOLDING ABOUT THE LINE IN FRONT OF THE MAKER? Asked before the
    /// list is drawn as well as before a candidate is taken, because a list drawn under a
    /// line it is not about is a wrong answer whether or not anybody presses Tab.
    ///
    /// ⚠ THE COST IS A PUBLICATION WITH NO LIST while a fresh answer is in flight, and it is
    /// the price of the seam: the completion used to be a function call and is a round trip
    /// now. DO NOT read "the host drains to idle" as "nobody sees it" -- the drain says the
    /// ask and its answer are spent in one turn, and says nothing about what the Skin was
    /// handed on the way. Both canvases are delivered in that same turn, and whether a medium
    /// draws both is the medium's business, unmeasured here. What is bought for it is that no
    /// candidate is ever offered against a line that is not on the screen.
    bool offer_applies() const { return offered_about_ == here(); }

    // ---- The rows, and the caret beside them ----------------------------------------------

    /// THE PANE, COMPOSED. Header, legend, transcript, omission, completion list, input row
    /// -- and the caret published beside them, on the input row.
    ///
    /// THE ROW BUDGET IS SPENT IN PRIORITY ORDER, because a pane can be granted any height a
    /// maker's arrangement gives it. The input row is first: a Terminal with no line is not a
    /// Terminal. Then a standing refusal, which is the answer to what the maker just did.
    /// Then the header (whose pane is this), then the omission marker (what am I not
    /// seeing), then the legend (what does `^` mean). What is left is split between the
    /// completion list and the transcript, and the list takes at most half -- the built-in's
    /// own share rule, which exists because a list that grew to fill the pane would answer
    /// the second question by erasing the first.
    ///
    /// ⚠ EVERY ROW HERE IS BUDGETED BEFORE IT IS COMPOSED, and that is the correction the
    /// notice taught: a row added after the budget was spent has to take one back, the row
    /// it takes back is the last one composed, and the last one composed is the input line.
    void say(loom::Mail& mail) {
        if (!granted_) {
            return; // no room has been sent: there is nothing this pane could truthfully fill
        }
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
                .send_to_role(kWorkshopRole, PaneContent{pane::kTerminalPane, std::move(out)});
            say_caret(mail);
            return;
        }
        // THE INPUT ROW IS TAKEN FIRST AND A NOTICE SECOND, BEFORE ANYTHING ELSE IS
        // COMPOSED. A refusal is the answer to the gesture the maker just made and belongs
        // beside the line it is about -- so it is part of the budget rather than something
        // added to a pane whose budget is already spent. It used to be the latter, and the
        // row it took back was the last one composed, which is the input row: the sentence
        // appeared and the line it was about vanished, with the caret.
        //
        // TWO ROWS IS THE SMALLEST ROOM THAT HOLDS BOTH. In one row there is no row for a
        // notice that is not the maker's own line, so the line keeps it and the refusal is
        // not shown -- the pane's stated order, followed to its end rather than abandoned at
        // the boundary.
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
        std::size_t list_wanted =
            list_open ? offered_.candidates.size() + 1 /*the heading*/ : 0;
        const std::size_t list_ceiling = static_cast<std::size_t>(rest / 2);
        if (list_wanted > list_ceiling) {
            list_wanted = list_ceiling;
        }
        // A LIST OF ONE ROW IS A HEADING WITH NO CANDIDATES UNDER IT, and that is a complete
        // answer rather than an empty box -- `kCompletionMinRows` was one for exactly this
        // reason, measured: with a floor of two, `send * s` showed nothing at all.
        if (list_open && list_wanted == 0 && rest >= 2) {
            list_wanted = 1;
        }
        const std::int64_t transcript_rows = rest - static_cast<std::int64_t>(list_wanted);

        // THE TRANSCRIPT, WRAPPED -- one entry becomes as many rows as its sentence needs.
        std::int64_t shown_entries = 0;
        if (transcript_rows > 0) {
            const std::size_t fits =
                entries_that_fit(known_.entries, columns_,
                                 static_cast<std::size_t>(transcript_rows));
            shown_entries = static_cast<std::int64_t>(fits);
            std::vector<std::string> lines;
            for (std::size_t i = known_.entries.size() - fits; i < known_.entries.size(); ++i) {
                for (std::string& one : entry_wrapped(known_.entries[i], columns_)) {
                    lines.push_back(std::move(one));
                }
            }
            if (static_cast<std::int64_t>(lines.size()) > transcript_rows) {
                lines.resize(static_cast<std::size_t>(transcript_rows));
            }
            for (std::int64_t i = 0; i < transcript_rows; ++i) {
                push(i < static_cast<std::int64_t>(lines.size()) ? lines[static_cast<std::size_t>(i)]
                                                                 : std::string(),
                     surface::role::kFill);
            }
        }
        if (omission) {
            // `earlier` IS THIS PANE'S ARITHMETIC and could be nobody else's: it is the
            // record's size less what THIS pane decided it could show whole.
            push(omission_text(static_cast<std::int64_t>(known_.entries.size()) - shown_entries,
                               known_.dropped),
                 surface::role::kMuted);
        }
        if (list_wanted > 0) {
            list_first_row_ = static_cast<std::int64_t>(out.size());
            list_row_count_ = static_cast<std::int64_t>(list_wanted);
            say_list(list_wanted, push);
        }

        // THE LINE BEING TYPED, AND -- while there is nothing on it -- the gesture that
        // answers "what can I say here". It is on this row rather than in the legend because
        // it is about what to do NEXT rather than about what a word means, and because it
        // erases itself: the moment a maker types anything the line has their text on it.
        const std::int64_t visible = columns_ - kPromptCols - kCaretCols;
        line_.keep_caret_visible(visible > 0 ? visible : 0);
        input_row_ = static_cast<std::int64_t>(out.size());
        const bool prompting = line_.empty() && !list_open;
        push(prompting ? std::string(">    Tab: what can this terminal say?")
                       : "> " + line_.visible(visible > 0 ? visible : 0),
             known_.attached ? surface::role::kAccent : surface::role::kAlert);

        if (static_cast<std::int64_t>(out.size()) > rows_) {
            out.resize(static_cast<std::size_t>(rows_));
        }
        if (input_row_ >= static_cast<std::int64_t>(out.size())) {
            input_row_ = -1; // cut away: there is no row to put a caret on
        }
        (void)mail.as_role(pane::kTerminalPaneRole)
            .send_to_role(kWorkshopRole, PaneContent{pane::kTerminalPane, std::move(out)});
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

    /// THE LINE BEING TYPED -- and the caret in it, the selection, and which part of it the
    /// row is showing. Its TEXT is the one thing a reload keeps.
    component::TextBox line_;

    /// WHAT THE PARTICIPANT SAID COULD COME NEXT, and which of it the maker is standing on.
    TerminalCompletionOffered offered_;
    Asking offered_about_;
    std::size_t selected_ = 0;
    bool dismissed_ = false;
    std::string dismissed_at_;
    bool asked_for_list_ = false;

    component::Clipboard clip_;
    struct Paste {
        bool awaiting = false;
        std::uint64_t pending = 0;
        /// THE DRAFT THAT ASKED (`component::TextBox::draft_epoch`). The correlation says
        /// this answer is to this pane's own ask; the epoch says the line it was asked for
        /// still exists. Workshop held exactly this field while the terminal line was a box
        /// of the host's, and losing it in the move is what QR-11's law is about.
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
    std::uint64_t asked_ = 0;
    bool acting_ = false;
    std::uint64_t act_pending_ = 0;
    bool completing_ = false;
    std::uint64_t completion_pending_ = 0;
    Asking asked_about_;
    bool stale_ = false;

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(TerminalPaneWeave)
