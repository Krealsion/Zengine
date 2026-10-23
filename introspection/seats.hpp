// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_SEATS_HPP
#define ZENGINE_INTROSPECTION_SEATS_HPP

// The three panes of the Introspection weave, each in a seat of its own: the rooms it holds, the
// question it has outstanding and the pictures it says (`PaneSeat`), then what each pane knows and
// does -- Loaded's selection and scroll, Project's reading, and Powers' search, cursor, sample,
// carry and clipboard. The weave (introspection.cpp) checks who is speaking, routes each delivery
// to its seat and counts what the seat says happened. Nothing here crosses a reload.
// Pane law: agents/panes.md

#include "loaded.hpp"
#include "powers.hpp"
#include "resolved.hpp"
#include "vocabulary.hpp"

#include "component/list_window.hpp"
#include "component/text_box.hpp"
#include "inventory/codec.hpp"
#include "input/vocabulary.hpp"
#include "operator/reference.hpp"
#include "surface/vocabulary.hpp"
#include "graphics/vocabulary.hpp"
#include "workshop/arrangement_vocabulary.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_parts.hpp"
#include "workshop/pane_seat.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/powers_vocabulary.hpp"
#include "workshop/sample_vocabulary.hpp"

#include <zen/kernel/manager.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::introspection {

/// The office Workshop holds, spelled as a string: a provider is a stranger to Workshop's
/// internals and names who it talks to as a third party would.
inline constexpr const char* kWorkshopRole = "zengine.workshop";

/// WHAT A SEAT SPEAKS WITH ON ONE DELIVERY: the mail it answers, the weave's own id (the sender a
/// value sent as a message names), and the one counter every question this office asks is
/// numbered from, for all three panes -- so a refusal's correlation matches one question at most.
struct Voice {
    loom::Mail& mail;
    loom::WeaveId self;
    std::uint64_t& asked;

    std::uint64_t next() { return ++asked; }
};

/// ONE PANE'S SEAT: its area (`workshop::PaneCanvasSeat`) and the question it has outstanding for
/// the room it holds. Transient, as every room is.
class PaneSeat {
public:
    explicit PaneSeat(const char* key) : room_(key, kIntrospectionRole) {}

    /// THE PANE'S AREA ITSELF, where a graphics window that shows it finds the pane.
    workshop::PaneCanvasSeat& room() noexcept { return room_; }

    /// The owner declined, or the ask reached nobody: the question is retired and nothing is said.
    void retire(std::uint64_t correlation) {
        if (awaiting_ && correlation == pending_) {
            awaiting_ = false;
        }
    }

protected:
    /// The room the pane's next reading is projected into: the rows and columns it holds.
    std::int64_t rows() const noexcept { return room_.rows(); }
    std::int64_t columns() const noexcept { return room_.columns(); }

    /// A NEW QUESTION OUTSTANDING, numbered from the office's one counter: a newer grant replaces
    /// the correlation, so the answer to an older room is dropped.
    std::uint64_t pose(Voice voice) {
        pending_ = voice.next();
        awaiting_ = true;
        return pending_;
    }

    /// Ask this pane's owner its question for the room it holds, by role (the addresses that
    /// survive their holders), as this office, because the arrangement door answers offices
    /// (workshop/arrangement.hpp).
    template <class Question>
    void ask(Voice voice, const char* owner, const Question& question) {
        const std::uint64_t correlation = pose(voice);
        (void)voice.mail.as_role(kIntrospectionRole).send_to_role(owner, question, correlation);
    }

    /// Is this the answer the pane waits on? `answers_ask()` first, which cannot be forged, then
    /// the correlation for which grant; the question is settled when it is.
    bool answering(const loom::Mail& mail) {
        return mail.answers_ask() && relayed(mail);
    }

    /// ...or, from an owner that relays personally and so stamps no answer, the correlation alone:
    /// all an asker can check.
    bool relayed(const loom::Mail& mail) {
        if (!awaiting_ || mail.correlation() != pending_) {
            return false;
        }
        awaiting_ = false;
        return true;
    }

    bool awaiting() const noexcept { return awaiting_; }

    /// SAY WHAT THE PANE SHOWS, the one place its content leaves: its rows, their parts and the caret
    /// standing in them, drawn in its area.
    void say_rows(loom::Mail& mail, std::vector<workshop::TextRow> shown,
                  std::vector<workshop::RowPart> parts = {}, const workshop::RowsCaret& caret = {}) {
        room_.publish_rows(mail, std::move(shown), std::move(parts), caret);
    }

private:
    workshop::PaneCanvasSeat room_;
    std::uint64_t pending_ = 0;
    bool awaiting_ = false;
};

// =============================================================================
// Loaded: the Kernel's loaded() map
// =============================================================================

/// THE LOADED PANE: the Manager's map, asked at every grant and every scroll, projected into the
/// room and spent. What it keeps is the presentation, bounded by the room and emptied at every
/// grant, and the weaver's selection, by the library name the row showed -- a name survives a
/// resize, a row does not. Nothing else in the process has one.
class LoadedSeat : public PaneSeat {
public:
    LoadedSeat() : PaneSeat(kLoadedPane) {}

    static workshop::v2::PaneOffered offering() {
        return workshop::v2::PaneOffered{kLoadedPane, kLoadedPaneName, kLoadedPaneSummary, 7, 54};
    }

    /// A ROOM GRANTED: the projection goes with the old room, so no press is read against rows no
    /// longer shown; the selected identity stays, and the Manager is asked again.
    void grant(Voice voice) {
        view_ = LoadedView{};
        ask(voice, loom::kManagerRole, loom::ListLoaded{});
    }

    /// The Manager's answer, matched on the correlation this office minted and an open question --
    /// all an asker can check, since the Manager relays personally: no authored role, no expected
    /// sender. A weave able to send `zen.Result` with a live private correlation could supply
    /// rows; that is the process tier's problem, not a claim this seam makes. A selection is
    /// cleared only here, when a reading's whole population lacks it, and clearing publishes
    /// nothing: a departure is not a weaver's gesture. True when it was this pane's reading.
    bool on_result(const loom::Result& r, loom::Mail& mail) {
        if (!relayed(mail)) {
            return false; // an answer to a question this pane did not ask
        }
        const std::vector<LoadedWeave> read = parse_loaded(r.value);
        if (!names(read, selected_)) {
            selected_.clear();
        }
        total_ = static_cast<std::int64_t>(read.size());
        view_ = project_loaded(read, rows(), columns(), static_cast<std::size_t>(origin_));
        origin_ = std::min(origin_, total_ - static_cast<std::int64_t>(view_.shown.size()));
        mark_selected(view_, selected_, columns());
        say(mail);
        return true;
    }

    /// A WEAVER PRESSED A ROW: a gesture becomes a fact, read against the projection on screen with
    /// nothing re-asked -- re-reading the Manager here could select something the weaver was never
    /// shown. A row naming no entry selects nothing and clears nothing. The same row pressed twice
    /// publishes twice (a selection is an occurrence) and re-sends no picture. Published, not
    /// addressed: who ought to care is not this pane's decision; as this office, because a fact
    /// about a weaver's gesture is worth what its office is. True when it published.
    bool press(std::int64_t pressed_row, loom::Mail& mail) {
        const LoadedWeave* entry = entry_at_row(view_, pressed_row);
        if (entry == nullptr) {
            return false; // a heading, a note, an omission marker, a blank, or no projection at all
        }
        const bool moved = selected_ != entry->name;
        selected_ = entry->name;
        const std::string role = entry->role;
        if (moved) {
            mark_selected(view_, selected_, columns());
            say(mail);
        }
        (void)mail.as_role(kIntrospectionRole)
            .publish(LoadedSelected{kLoadedPane, selected_, role});
        return true;
    }

    /// SCROLLING CHANGES THE VIEWPORT, never the selected identity: the owner is read again rather
    /// than an independently authoritative population retained here.
    void wheel(Voice voice, double dy) {
        if (awaiting() || !std::isfinite(dy)) {
            return;
        }
        const std::int64_t steps =
            component::take_wheel_notches(wheel_, std::clamp(dy, -1000.0, 1000.0));
        const auto next = std::clamp(origin_ - steps, std::int64_t{0},
                                     std::max(std::int64_t{0}, total_ - 1));
        if (next == origin_) {
            return;
        }
        origin_ = next;
        ask(voice, loom::kManagerRole, loom::ListLoaded{});
    }

    /// Escape's default: the chosen row is let go and the pane repainted; false when there was
    /// none. Like any clearing it publishes nothing.
    bool drop_selection(loom::Mail& mail) {
        if (selected_.empty()) {
            return false;
        }
        selected_.clear();
        mark_selected(view_, selected_, columns());
        say(mail);
        return true;
    }

    /// WHAT LOADED CALLS ITS ROWS: a loaded weave's row by its name, `weave:<name>`.
    std::vector<workshop::RowPart> loaded_parts() const {
        workshop::PartNames<workshop::RowPart> named;
        for (std::size_t row = 0; row < view_.rows.size(); ++row) {
            if (const LoadedWeave* entry = entry_at_row(view_, static_cast<std::int64_t>(row))) {
                (void)named.add(workshop::RowPart{
                    "weave:" + entry->name, static_cast<std::int64_t>(row), 0, columns()});
            }
        }
        return named.take();
    }

private:
    /// Loaded's rows, each press meaning the weave it names.
    void say(loom::Mail& mail) {
        say_rows(mail, view_.rows, loaded_parts());
    }

    LoadedView view_;
    std::string selected_;
    std::int64_t origin_ = 0, total_ = 0;
    double wheel_ = 0;
};

// =============================================================================
// Project: the realization owner's plan and rows
// =============================================================================

/// THE PROJECT PANE: the host's arrangement, asked at every grant, spent and dropped -- nothing
/// retains a row. Read-only: a press there gives it the keys and says nothing.
class ArrangementSeat : public PaneSeat {
public:
    ArrangementSeat() : PaneSeat(kArrangementPane) {}

    static workshop::v2::PaneOffered offering() {
        return workshop::v2::PaneOffered{kArrangementPane, kArrangementPaneName,
                                         kArrangementPaneSummary, 9, 64};
    }

    void grant(Voice voice) {
        ask(voice, workshop::kArrangementRole, workshop::ArrangementRequested{});
    }

    /// The host's answer about its arrangement, in either version -- version 2 carries every row's
    /// state, and for a row that is not running its reason and the next step. `answers_ask()`
    /// first, provenance the bus attaches and no payload can write, then the correlation, so a late
    /// answer to an older room is dropped. True when it was this pane's reading.
    template <class Resolved>
    bool on_resolved(const Resolved& said, loom::Mail& mail) {
        if (!answering(mail)) {
            return false;
        }
        say_rows(mail, project_arrangement(said, rows(), columns()));
        return true;
    }
};

// =============================================================================
// Powers: the host's operator catalog, as a weaver uses it
// =============================================================================

/// THE POWERS PANE: the weaver's side of it (`PowersUi`, whose members powers.hpp states the law
/// of), what it shows and what each place means, its one outstanding sample and describe, a press
/// a drag may yet carry, and the clipboard its query reads. All of it transient; the reading is the
/// host's only words in here, replaced whole at every grant.
class PowersSeat : public PaneSeat {
public:
    PowersSeat() : PaneSeat(kPowersPane) {}

    static workshop::v2::PaneOffered offering() {
        return workshop::v2::PaneOffered{kPowersPane, kPowersPaneName, kPowersPaneSummary, 8, 58};
    }

    /// WHAT POWERS ANSWERS TO: its actions with their shipped defaults (`input::scan` and
    /// `input::mod` numbers), each with what its press may ask the door or the sampler -- what
    /// `on_action` reads. Applying a weaver's keymap is Workshop's.
    static workshop::v3::PaneActions actions() {
        const workshop::ActionCause found{
            workshop::find_powers_schema()->name(),
            static_cast<std::int64_t>(workshop::find_powers_schema()->version()), {}};
        const workshop::ActionCause described = workshop::cause_of<workshop::DescribePower>();
        workshop::v3::PaneActions declared;
        declared.pane = kPowersPane;
        const auto row = [&declared](const char* id, const char* label, std::int64_t sc,
                                     std::vector<workshop::ActionCause> causes) {
            declared.rows.push_back(workshop::v3::PaneActionRow{id, label, sc, input::mod::kNone,
                                                                {}, std::move(causes)});
        };
        row(kPowersActionView, "switch view", input::scan::kTab, {found, described});
        row(kPowersActionUp, "row up", input::scan::kUp, {described});
        row(kPowersActionDown, "row down", input::scan::kDown, {described});
        row(kPowersActionSample, "sample", input::scan::kReturn,
            {workshop::cause_of<workshop::SampleRequested>()});
        row(kPowersActionSources, "show sources", input::scan::kUnknown, {found, described});
        row(kPowersActionOperators, "show operators", input::scan::kUnknown, {found, described});
        row(kPowersActionComposite, "composite only, on or off", input::scan::kUnknown, {found});
        return declared;
    }

    /// THE LAYOUT'S CHROME WIDTHS: its row is said again where it shows, since its controls move.
    void take_widths(const ChromeWidths& widths, loom::Mail& mail) {
        if (widths == ui_.widths) {
            return;
        }
        ui_.widths = widths;
        say_powers(mail);
    }

    /// A ROOM GRANTED: the reading goes with the projection, since until the door answers there is
    /// nothing to derive a view from or read a press against. What the weaver authored survives --
    /// the view, the query, the filter, both selections and the retained sample -- and the search
    /// they make is asked again, with the selected power's description.
    void grant(Voice voice) {
        ui_.reading = workshop::PowersFound{};
        ui_.read = false;
        shown_ = PowersView{};
        ask_powers(voice);
        ask_described(voice);
    }

    /// The door's answer to this pane's search, matched as the arrangement's is, so an answer to a
    /// search the weaver has typed past is dropped. This subject moves (an overlay changes a
    /// power's contribution) and nothing here is told; the next grant or search asks again. The
    /// answer is retained for the cursor between asks, never consulted for what is true now. A
    /// refusal is the door's sentence, shown as the list. True when it was this pane's reading.
    bool on_found(const workshop::PowersFound& said, loom::Mail& mail) {
        if (!answering(mail)) {
            return false;
        }
        ui_.reading = said;
        ui_.read = true;
        say_powers(mail);
        return true;
    }

    /// What the door said of the selected identity: its row and its whole contribution stack, and
    /// the one answer that may clear a selection -- the identity is gone (`take_described`).
    /// Matched by `answers_ask()` and the correlation, so a describe the weaver has moved past
    /// cannot relabel another selection.
    void on_described(const workshop::PowerDescribed& said, loom::Mail& mail) {
        if (!describing_.awaiting || !mail.answers_ask() ||
            mail.correlation() != describing_.pending) {
            return;
        }
        describing_.awaiting = false;
        workshop::PowerDescribed taken = said;
        taken.identity = describing_.identity;
        take_described(ui_, std::move(taken));
        say_powers(mail);
    }

    /// What a Source said when this weaver asked, matched as the arrangement's is, so a newer ask
    /// drops an older answer. The identity kept is the one this pane asked for, never the
    /// payload's. Retained as history and never refreshed; its row leads with `sampled when asked`
    /// so the tense cannot be cut off.
    void on_sampled(const workshop::SourceSampled& said, loom::Mail& mail) {
        if (!sampling_.awaiting || !mail.answers_ask() || mail.correlation() != sampling_.pending) {
            return;
        }
        sampling_.awaiting = false;
        ui_.sample.present = true;
        ui_.sample.identity = sampling_.identity;
        ui_.sample.ok = said.ok;
        ui_.sample.reason = said.reason;
        ui_.sample.lines = said.lines;
        say_powers(mail);
    }

    /// A refusal of one of this pane's questions retires it; one counter mints every correlation,
    /// so at most one matches.
    void refused(std::uint64_t correlation) {
        retire(correlation);
        if (sampling_.awaiting && correlation == sampling_.pending) {
            sampling_.awaiting = false;
        }
        if (describing_.awaiting && correlation == describing_.pending) {
            describing_.awaiting = false;
        }
    }

    /// A KEY WHILE POWERS HOLDS THE KEYBOARD: only the query field's editing vocabulary acts here;
    /// what the pane commands arrives as a resolved id (`on_action`). A copy is said once; a paste
    /// is asked of the office that holds the platform (`begin_clipboard_paste`), since only an
    /// owner can read the clipboard's current value.
    void on_key(Voice voice, const workshop::PaneKey& key) {
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        const std::string searched = ui_.query.text();
        if (!ui_.query.consume(key.scancode, key.modifiers, clip_)) {
            return; // a key this pane's field has no word for changes nothing, says nothing
        }
        if (clip_.writes != copied_before) {
            voice.mail.publish(graphics::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_clipboard_paste(voice.mail);
        }
        if (ui_.query.text() != searched) {
            ask_powers(voice);
        }
        say_powers(voice.mail);
    }

    /// Escape's default: the selected power is let go and the pane repainted; false when there was
    /// none. Like any clearing it publishes nothing.
    bool drop_selection(loom::Mail& mail) {
        if (ui_.selected().empty()) {
            return false;
        }
        ui_.select(std::string());
        say_powers(mail);
        return true;
    }

    /// ONE OF THE PANE'S DECLARED ACTIONS, BY ITS RESOLVED ID: Workshop matched the keystroke
    /// against the weaver's keymap, so nothing here knows the key. True when it asked for a sample.
    bool on_action(Voice voice, const std::string& id) {
        if (id == kPowersActionView) {
            ui_.view = ui_.view == powers_view::kSources ? powers_view::kOperators
                                                         : powers_view::kSources;
            ask_powers(voice);
            ask_described(voice);
        } else if (id == kPowersActionUp || id == kPowersActionDown) {
            const std::string was = ui_.selected();
            move_cursor(ui_, id == kPowersActionUp ? -1 : +1);
            if (ui_.selected() != was) {
                ask_described(voice);
            }
        } else if (id == kPowersActionSources || id == kPowersActionOperators) {
            if (!show_view(voice, id == kPowersActionSources ? powers_view::kSources
                                                             : powers_view::kOperators)) {
                return false;
            }
        } else if (id == kPowersActionComposite) {
            toggle_composite(voice);
        } else if (id == kPowersActionSample) {
            // The one evaluation gesture; `sampleable` is empty for anything but a selected
            // Source, so the Operators view has no invocation path.
            return ask_sample(voice);
        } else {
            return false; // an id this pane never declared: Workshop sends none, and none acts
        }
        say_powers(voice.mail);
        return false;
    }

    /// The wheel walks the cursor a row a notch; fractional notches wait until they are worth one.
    void wheel(Voice voice, double dy) {
        const std::int64_t steps = component::take_wheel_notches(wheel_, dy);
        if (steps == 0) {
            return;
        }
        const std::string was = ui_.selected();
        move_cursor(ui_, -steps);
        if (ui_.selected() == was) {
            return; // at the edge, or nothing to walk: nothing moved, nothing is re-said
        }
        ask_described(voice);
        say_powers(voice.mail);
    }

    /// TYPED CHARACTERS: this pane's one field, so printable text is the query. Refused whole at
    /// this door unless every byte is printable ASCII, a row's contract: a character is an advance
    /// of the window's text. Every road into the query passes this door: typed text, a mirrored
    /// copy, a paste.
    void on_typed(Voice voice, const std::string& text) {
        if (text.empty() || !workshop::pane_text::admissible(text)) {
            return;
        }
        ui_.query.type(text);
        ask_powers(voice);
        say_powers(voice.mail);
    }

    /// A copy said anywhere in the process, mirrored so a Terminal copy pastes into the query and
    /// gated as typing is. `writes` counts only this pane's copies, so nothing echoes.
    void on_copy(const graphics::ClipboardCopy& said) {
        if (workshop::pane_text::admissible(said.text)) {
            clip_.text = said.text;
        }
    }

    /// The clipboard's answer to a paste this pane asked for, the one road foreign clipboard text
    /// has in: `answers_ask()` and the book's settlement, then the draft that asked must still be
    /// there (`draft_epoch`).
    void on_clipboard(Voice voice, const graphics::ClipboardText& a) {
        if (!voice.mail.answers_ask()) {
            return;
        }
        const std::optional<loom::PendingAsk> settled =
            clip_asks_.settle(voice.mail.correlation(), voice.mail.sender());
        if (!settled || !pasting_ || settled->id != paste_ask_) {
            return;
        }
        pasting_ = false;
        if (ui_.query.draft_epoch() != paste_epoch_) {
            return; // the query left the state that asked; the payload is discarded
        }
        if (a.readable) {
            if (!workshop::pane_text::admissible(a.text)) {
                return; // refused WHOLE, at this pane's door, for `on_typed`'s reason
            }
            clip_.text = a.text; // the platform's current truth, asked for by this paste
        }
        const std::string searched = ui_.query.text();
        ui_.query.paste(clip_);
        if (ui_.query.text() != searched) {
            ask_powers(voice);
        }
        say_powers(voice.mail);
    }

    /// A PRESS ON THE PANE'S PICTURE, resolved against what is on screen through the spans the
    /// projection returned. A row selects and never samples, so a cold pane's first press is one
    /// act; a press that changes nothing republishes nothing. True when it asked for a sample.
    bool press(Voice voice, std::int64_t pressed_row, std::int64_t pressed_column) {
        const PowersTarget hit = target_at(shown_, pressed_row, pressed_column);
        switch (hit.control) {
        case powers_control::kSources:
        case powers_control::kOperators:
            if (!show_view(voice, hit.control == powers_control::kSources
                                      ? powers_view::kSources
                                      : powers_view::kOperators)) {
                return false;
            }
            break;
        case powers_control::kComposite:
            toggle_composite(voice);
            break;
        case powers_control::kEntry:
            // A press on a power may become a drag carrying its reference: held until the hand
            // moves, so a click that never moved asks nothing.
            carry_ = Carry{};
            carry_.armed = true;
            carry_.row = pressed_row;
            carry_.column = pressed_column;
            carry_.gesture = voice.mail.correlation();
            carry_.identity = hit.identity;
            if (ui_.selected() == hit.identity) {
                return false;
            }
            ui_.select(hit.identity);
            ask_described(voice);
            break;
        case powers_control::kSample:
            return ask_sample(voice);
        default:
            // Anything else selects nothing and clears nothing: a miss is not a deselection.
            return false;
        }
        say_powers(voice.mail);
        return false;
    }

    /// THE HAND MOVED WITH THE BUTTON DOWN after a press on a power, onto another cell: its
    /// reference -- the identity and the two content ids the door's row carried -- is acquired
    /// under that press as a typed value, `zengine.OperatorRef`, for Workshop to carry. A form is
    /// no operator and carries none.
    void drag(Voice voice, std::int64_t dragged_row, std::int64_t dragged_column) {
        if (!carry_.armed || carry_.started ||
            (dragged_row == carry_.row && dragged_column == carry_.column)) {
            return;
        }
        carry_.started = true;
        const workshop::PowerRow* row = nullptr;
        for (const workshop::PowerRow& r : ui_.reading.rows) {
            if (r.identity == carry_.identity) {
                row = &r;
            }
        }
        if (row == nullptr || row->kind == workshop::kFormKind) {
            return;
        }
        const op::OperatorRef ref{row->identity,
                                  static_cast<loom::ContentId>(row->inputs.content_id),
                                  static_cast<loom::ContentId>(row->outputs.content_id)};
        const std::string bytes = inventory::encode_pair(op::encode_reference(ref), {});
        carry_.bytes.assign(bytes.begin(), bytes.end());
        carry_.ask = voice.next();
        (void)voice.mail.as_role(kIntrospectionRole)
            .send_to_role(kWorkshopRole,
                          workshop::PaneOperationRequested{
                              kPowersPane, kWorkshopRole,
                              workshop::PaneValueCarryRequested::zen_name, 1,
                              static_cast<std::int64_t>(carry_.gesture)},
                          carry_.ask);
    }

    /// The button let go, or the hand lost, before any drag: the press is spent.
    void release() {
        if (!carry_.started) {
            carry_ = Carry{};
        }
    }

    /// Workshop's answer to that acquisition: allowed, the reference is handed over to carry under
    /// the press's own number; refused, nothing is carried.
    void on_approval(const workshop::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask() || carry_.ask == 0 || mail.correlation() != carry_.ask) {
            return;
        }
        carry_.ask = 0;
        if (!answer.allowed) {
            return;
        }
        (void)mail.as_role(kIntrospectionRole)
            .send_to_role(kWorkshopRole,
                          workshop::PaneValueCarryRequested{kPowersPane, carry_.identity,
                                                            carry_.bytes, true},
                          carry_.gesture);
    }

    /// Carried or refused, the press is spent.
    void carried() { carry_ = Carry{}; }

    /// WHAT POWERS CALLS ITS PARTS: a power's row by its identity, `power:<identity>`, and each
    /// control by the action it performs, `control:<id>`.
    std::vector<workshop::RowPart> powers_parts() const {
        workshop::PartNames<workshop::RowPart> named;
        const auto control = [](const char* action) { return std::string("control:") + action; };
        for (const PowersSpan& span : shown_.spans) {
            std::string name;
            switch (span.control) {
            case powers_control::kSources: name = control(kPowersActionSources); break;
            case powers_control::kOperators: name = control(kPowersActionOperators); break;
            case powers_control::kComposite: name = control(kPowersActionComposite); break;
            case powers_control::kSample: name = control(kPowersActionSample); break;
            case powers_control::kEntry: name = "power:" + span.identity; break;
            default: break;
            }
            (void)named.add(workshop::RowPart{std::move(name), span.row, span.first,
                                                  span.last - span.first + 1});
        }
        return named.take();
    }

private:
    /// SAY WHAT POWERS SHOWS, the one place its projection is rebuilt: the caret's window is
    /// reconciled first against the capacity the chrome row cuts the query with. Silent with no
    /// room in force, or between a grant and its answer.
    void say_powers(loom::Mail& mail) {
        if (!ui_.read || rows() <= 0 || columns() <= 0) {
            return;
        }
        ui_.query.keep_caret_visible(query_capacity(ui_, columns()));
        shown_ = project_powers_ui(ui_, rows(), columns());
        workshop::RowsCaret caret;
        caret.row = shown_.caret_row;
        caret.column = shown_.caret_col;
        say_rows(mail, shown_.rows, powers_parts(), caret);
    }

    /// SHOW ONE VIEW, Sources or Operators, as its control and its action ask: false when it is
    /// shown already, which asks nothing.
    bool show_view(Voice voice, std::int64_t want) {
        if (ui_.view == want) {
            return false;
        }
        ui_.view = want;
        ask_powers(voice);
        ask_described(voice);
        return true;
    }

    /// ...AND TURN THE COMPOSITE FILTER over, as its control and its action ask.
    void toggle_composite(Voice voice) {
        ui_.composite_only = !ui_.composite_only;
        ask_powers(voice);
    }

    /// Ask the host to run one Source because a weaver said so: the only thing here that can
    /// cause an evaluation, from `Return` and `[ Sample ]` alone. One outstanding: a second
    /// gesture replaces the correlation, and the door still spends both. True when it asked.
    bool ask_sample(Voice voice) {
        std::string identity = sampleable(ui_);
        if (identity.empty()) {
            return false; // nothing selected, or the selection is an Operator: no gesture exists
        }
        sampling_.pending = voice.next();
        sampling_.awaiting = true;
        sampling_.identity = identity;
        (void)voice.mail.as_role(kIntrospectionRole)
            .send_to_role(workshop::kSampleRole, workshop::SampleRequested{std::move(identity)},
                          sampling_.pending);
        return true;
    }

    /// Ask the door this pane's search, as this office: a newer search replaces the correlation,
    /// so the answer to one the weaver has typed past is dropped. `FindPowers` is a value built
    /// at its own schema, its fields optional, so it leaves as that value.
    void ask_powers(Voice voice) {
        const std::uint64_t correlation = pose(voice);
        (void)voice.mail.bus().office_send_to_role(
            kIntrospectionRole, workshop::kPowersRole,
            loom::Message(workshop::find_powers_value(powers_question(ui_)), voice.self,
                          loom::WeaveId{}, correlation));
    }

    /// Ask the door about the selected identity: its row and every contribution eligible to
    /// satisfy it. With nothing selected, nothing is asked.
    void ask_described(Voice voice) {
        const std::string& identity = ui_.selected();
        if (identity.empty()) {
            return;
        }
        describing_.pending = voice.next();
        describing_.awaiting = true;
        describing_.identity = identity;
        (void)voice.mail.as_role(kIntrospectionRole)
            .send_to_role(workshop::kPowersRole, workshop::DescribePower{identity},
                          describing_.pending);
    }

    /// Ask what the platform clipboard holds, of the window that shows the pane's area, else the
    /// Skin, for a paste the query requested; the epoch says the draft the answer is for is still
    /// in the box.
    void begin_clipboard_paste(loom::Mail& mail) {
        const std::string office = room().clipboard_office();
        const loom::AskOpened opened = clip_asks_.open_to_role(
            office, graphics::ClipboardTextRequested::zen_name,
            graphics::ClipboardTextRequested::zen_version);
        if (!opened) {
            return;
        }
        paste_ask_ = opened.id;
        paste_epoch_ = ui_.query.draft_epoch();
        pasting_ = true;
        (void)mail.as_role(kIntrospectionRole)
            .send_to_role(office, graphics::ClipboardTextRequested{}, opened.correlation);
    }

    /// ONE OUTSTANDING QUESTION OF ITS OWN, and the identity it asked about: a sample or a
    /// describe belongs to no room, and giving it rows and columns it would never spend would
    /// invite one of them to be read. Its answer is kept as being about that identity, never
    /// whatever is selected when it lands.
    struct Outstanding {
        std::uint64_t pending = 0;
        bool awaiting = false;
        std::string identity;
    };

    /// A press on a power that a drag may yet carry: where it was, under which input, which power,
    /// and once the hand moved, the acquisition asked and the reference's bytes.
    struct Carry {
        bool armed = false, started = false;
        std::int64_t row = 0, column = 0;
        std::uint64_t gesture = 0, ask = 0;
        std::string identity;
        loom::Bytes bytes;
    };

    PowersUi ui_;
    /// What Powers shows and what its places mean: presentation, emptied at every grant.
    PowersView shown_;
    /// Fractional wheel notches not yet worth a row; transient like the cursor.
    double wheel_ = 0.0;
    Outstanding sampling_;
    Outstanding describing_;
    Carry carry_;
    /// THE PROCESS'S COPIED TEXT AS THIS PANE LAST HEARD IT, and the book for the one paste
    /// conversation it can hold open. Two, because a clipboard is a mirror and an ask is a
    /// question -- the Composer's shipped pair, for its reasons.
    component::Clipboard clip_;
    loom::AskBook clip_asks_{1};
    std::uint64_t paste_ask_ = 0;
    std::uint64_t paste_epoch_ = 0;
    bool pasting_ = false;
};

} // namespace zengine::introspection

#endif // ZENGINE_INTROSPECTION_SEATS_HPP
