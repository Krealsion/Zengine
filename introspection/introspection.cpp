// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Introspection provider: a loadable weave offering Workshop three read-only panes about the
// running system, each with its own owner -- `loaded` (the Kernel's loaded() map), `arrangement`
// (the realization owner's plan and rows) and `powers` (the host's operator catalog). They
// disagree on purpose: a provider is a row of `arrangement` and absent from `loaded`. Every fact
// is asked for, spent and dropped; a room grant re-reads it, since nothing can be subscribed to.
// Pane law: agents/panes.md

// It cannot load, unload, mount or evaluate: its sends are the pane protocol, `zen.ListLoaded`
// (enumeration, not the load capability), `LoadedSelected`, the arrangement question, the
// discovery door's two asks, one `SampleRequested` per weaver gesture, and the clipboard pair. It
// links no operator target, so browsing cannot evaluate. The loader binds `allow_any()` to every
// library, so this is a claim about what the weave does, not containment.
// Reference: docs/reference/introspection.md.

#include "loaded.hpp"
#include "powers.hpp"
#include "resolved.hpp"
#include "vocabulary.hpp"

#include "activation/activation.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "inventory/codec.hpp"
#include "operator/reference.hpp"
#include "workshop/arrangement_vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_escape.hpp"
#include "workshop/pane_parts.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/powers_vocabulary.hpp"
#include "workshop/sample_vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace component = zengine::component;
namespace input = zengine::input;
namespace surface = zengine::surface;
namespace intro = zengine::introspection;
using zengine::introspection::kArrangementPane;
using zengine::introspection::kArrangementPaneName;
using zengine::introspection::kArrangementPaneSummary;
using zengine::introspection::kIntrospectionRole;
using zengine::introspection::kLoadedPane;
using zengine::introspection::kLoadedPaneName;
using zengine::introspection::kLoadedPaneSummary;
using zengine::introspection::kPowersActionDown;
using zengine::introspection::kPowersActionSample;
using zengine::introspection::kPowersActionUp;
using zengine::introspection::kPowersActionView;
using zengine::introspection::kPowersPane;
using zengine::introspection::kPowersPaneName;
using zengine::introspection::kPowersPaneSummary;
using zengine::introspection::LoadedSelected;
using zengine::introspection::LoadedView;
using zengine::introspection::LoadedWeave;
using zengine::workshop::ArrangementRequested;
using zengine::workshop::DescribePower;
using zengine::workshop::kArrangementRole;
using zengine::workshop::kPowersRole;
using zengine::workshop::kSampleRole;
using zengine::workshop::PaneActionRequested;
using zengine::workshop::PaneActionRow;
using zengine::workshop::PaneActions;
using zengine::workshop::PaneCatalogRequested;
using zengine::workshop::PaneKey;
using zengine::workshop::v2::PaneOffered;
using zengine::workshop::PaneRoom;
using zengine::workshop::PaneTextInput;
using zengine::workshop::PowerDescribed;
using zengine::workshop::PowersFound;
using zengine::workshop::ResolvedArrangement;
using zengine::workshop::SampleRequested;
using zengine::workshop::SourceSampled;

/// The office Workshop holds, spelled as a string: a provider is a stranger to Workshop's
/// internals and names who it talks to as a third party would.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// What this provider has done, and it is all counters: the loaded map, the resolved rows and
/// the contribution stacks belong to their owners, and a copy here would be the answer that goes
/// stale. The selection and the Powers pane's interaction are transient members below, never
/// snapshotted: a revived incarnation has no room, and nothing a selection could be of.
struct IntrospectionState {
    std::int64_t offers = 0;
    std::int64_t rooms = 0;
    std::int64_t readings = 0;   ///< answers that became content, from any of the three owners
    std::int64_t refused = 0;    ///< asks, rooms and presses not authored by the Workshop office
    std::int64_t selections = 0; ///< weaver selections published as `LoadedSelected`
    std::int64_t samples = 0;    ///< explicit weaver sample gestures this office asked for
    ZEN_EXPOSE();
    ZEN_SHAPE(IntrospectionState, 3, ZEN_FIELD(offers), ZEN_FIELD(rooms), ZEN_FIELD(readings),
              ZEN_FIELD(refused), ZEN_FIELD(selections), ZEN_FIELD(samples));
};

class IntrospectionWeave
    : public loom::WeaveBase<
          IntrospectionWeave, IntrospectionState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom,
                       zengine::workshop::PaneCanvasRoom, zengine::workshop::PaneCanvasPointer,
                       zengine::workshop::PaneCanvasRejected, PaneKey, PaneTextInput,
                       PaneActionRequested, loom::Result, zengine::workshop::PaneOperationAnswered,
                       zengine::workshop::PaneCarryAnswered,
                       loom::Refused, ResolvedArrangement, zengine::workshop::v2::ResolvedArrangement,
                       PowersFound, PowerDescribed, SourceSampled,
                       surface::ClipboardCopy, surface::ClipboardText>,
          loom::Emit<PaneOffered, PaneActions, zengine::workshop::v4::PaneContent,
                     zengine::workshop::v5::PaneCanvasContent, zengine::workshop::PaneCaret,
                     zengine::workshop::PanePassRequested, LoadedSelected, loom::ListLoaded,
                     ArrangementRequested, DescribePower, SampleRequested,
                     zengine::workshop::PaneOperationRequested,
                     zengine::workshop::PaneValueCarryRequested,
                     zengine::workshop::PaneEscapeUnspent,
                     surface::ClipboardCopy, surface::ClipboardTextRequested>> {
public:
    /// First breath, only if Loom says so: `ActivationCursor` requires Loom's attestation and a
    /// sequence not yet acted on, so no weave can make a pane appear by sending `zen.Activated`.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        announce(mail);
    }

    /// Workshop asking who has panes. `authored_from_role`, not `sender()`: the ask is a
    /// publication, and only Loom's stamp on its authorship says it was Workshop.
    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        announce(mail);
    }

    /// Workshop granting one pane its prose budget: kept for a host that grants no canvas, and
    /// spent only while the pane holds no canvas room, whose lattice is the budget then.
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return; // a forged room grants nothing and produces no content
        }
        Canvas* canvas = canvas_of(room.pane);
        if (canvas == nullptr) {
            return; // a room for a pane this provider does not have: neither counted nor answered
        }
        canvas->prose_rows = room.rows;
        canvas->prose_columns = room.columns;
        if (!canvas->on()) {
            grant(mail, room);
        }
    }

    /// THE PANE'S OWN CANVAS: while it holds a room there it draws its rows as its picture, and
    /// the lattice's rows and columns are its budget. A room of no extent leaves the pane to its
    /// prose room. Every grant is a room grant (`grant`), the picture's numbers its own.
    void on(const zengine::workshop::PaneCanvasRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        Canvas* canvas = canvas_of(room.pane);
        if (canvas == nullptr) {
            return;
        }
        canvas->room = room;
        const zengine::workshop::CanvasRows lattice = zengine::workshop::canvas_rows(room);
        if (canvas->on()) {
            grant(mail, PaneRoom{room.pane, lattice.rows, lattice.columns});
        } else if (canvas->prose_rows > 0 && canvas->prose_columns > 0) {
            grant(mail, PaneRoom{room.pane, canvas->prose_rows, canvas->prose_columns});
        }
    }

    /// A refused picture leaves the last good one showing; the next reading draws again.
    void on(const zengine::workshop::PaneCanvasRejected&, loom::Mail&) {}

    /// A HAND ON A PANE'S PICTURE, read back to the row and column of its lattice: a primary
    /// press is the press a row was, its motion a drag, the wheel the wheel; a right press is
    /// handed back, so Workshop's own pane menu opens where it was made. Only for the room the
    /// pane holds: a pointer naming another grant is from a room since replaced, and a primary
    /// press naming a picture drawn under another press map is dropped -- never read against
    /// whatever row has since moved into its place.
    void on(const zengine::workshop::PaneCanvasPointer& event, loom::Mail& mail) {
        namespace cp = zengine::workshop::canvas_pointer;
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        Canvas* canvas = canvas_of(event.pane);
        if (canvas == nullptr || !canvas->on() || event.grant != canvas->room.grant) {
            return;
        }
        const zengine::workshop::RowCell at = zengine::workshop::row_cell_at(
            zengine::workshop::canvas_rows(canvas->room), event.x, event.y);
        if (event.phase == cp::kWheel) {
            wheel(event.pane, event.dy, mail);
            return;
        }
        if (event.phase == cp::kMove) {
            if (event.pane == kPowersPane) powers_drag(at.row, at.column, mail);
            return;
        }
        if (event.phase == cp::kRelease || event.phase == cp::kLost) {
            if (event.pane == kPowersPane && !carry_.started) carry_ = Carry{};
            return;
        }
        if (event.phase != cp::kPress) {
            return;
        }
        if (event.button == 3) {
            (void)zengine::workshop::pane_menu::pass_back(mail, kIntrospectionRole, event.pane);
            return;
        }
        if (event.button != 1 || !at.shown ||
            !canvas->pictures.current(event.grant, event.picture)) {
            return;
        }
        if (event.pane == kPowersPane) {
            on_powers_press(at.row, at.column, mail);
        } else if (event.pane == kLoadedPane) {
            loaded_press(at.row, mail);
        }
        // `arrangement` is read-only: a press there gives it the keys and says nothing.
    }

private:
    /// Workshop granting one pane its budget, from either room. Each pane keeps its own room and
    /// outstanding question (`Asked`), or the last grant would decide how the others are drawn.
    /// The room is kept and the owner asked; content goes when the owner answers, never from a
    /// previous reading -- Workshop clears its cache before every grant, and its `(waiting for
    /// the provider)` is the honest gap. The row map goes with the old projection, so no press
    /// is read against rows no longer shown; the selected identity stays.
    void grant(loom::Mail& mail, const PaneRoom& room) {
        if (room.pane == kLoadedPane) {
            ++state_.rooms;
            view_ = LoadedView{};
            ask(mail, loaded_, room, loom::kManagerRole, loom::ListLoaded{});
        } else if (room.pane == kArrangementPane) {
            ++state_.rooms;
            ask(mail, arrangement_, room, kArrangementRole, ArrangementRequested{});
        } else if (room.pane == kPowersPane) {
            ++state_.rooms;
            // The reading goes with the projection: until the door answers there is nothing to
            // derive a view from or read a press against. What the weaver authored survives --
            // the view, the query, the filter, both selections and the retained sample -- and
            // the search they make is asked again, with the selected power's description.
            powers_ui_.reading = PowersFound{};
            powers_ui_.read = false;
            powers_shown_ = intro::PowersView{};
            powers_.rows = room.rows;
            powers_.columns = room.columns;
            ask_powers(mail);
            ask_described(mail);
        }
    }

    /// A weaver pressed a row of Loaded: a gesture becomes a fact, read against the projection on
    /// screen with nothing re-asked -- re-reading the Manager here could select something the
    /// weaver was never shown. A row naming no entry selects nothing and clears nothing. The same
    /// row pressed twice publishes twice (a selection is an occurrence) and re-sends no picture.
    void loaded_press(std::int64_t pressed_row, loom::Mail& mail) {
        const LoadedWeave* entry = zengine::introspection::entry_at_row(view_, pressed_row);
        if (entry == nullptr) {
            return; // a heading, a note, an omission marker, a blank, or no projection at all
        }
        const bool moved = selected_ != entry->name;
        selected_ = entry->name;
        const std::string role = entry->role;
        if (moved) {
            zengine::introspection::mark_selected(view_, selected_, loaded_.columns);
            say_rows(mail, kLoadedPane, view_.rows, loaded_parts());
        }
        ++state_.selections;
        // Published, not addressed: who ought to care is not this pane's decision. As this
        // office, because a fact about a weaver's gesture is worth what its office is.
        (void)mail.as_role(kIntrospectionRole)
            .publish(LoadedSelected{kLoadedPane, selected_, role});
    }

public:

    /// The Manager's answer, matched on the correlation this weave minted and an open question --
    /// all an asker can check, since the Manager relays personally: no authored role, no expected
    /// sender. A weave able to send `zen.Result` with a live private correlation could supply
    /// rows; that is the process tier's problem, not a claim this seam makes. A selection is
    /// cleared only here, when a reading's whole population lacks it, and clearing publishes
    /// nothing: a departure is not a weaver's gesture.
    void on(const loom::Result& r, loom::Mail& mail) {
        if (!loaded_.awaiting || mail.correlation() != loaded_.pending) {
            return; // an answer to a question this weave did not ask
        }
        loaded_.awaiting = false;
        ++state_.readings;
        const std::vector<LoadedWeave> read = zengine::introspection::parse_loaded(r.value);
        if (!zengine::introspection::names(read, selected_)) {
            selected_.clear();
        }
        loaded_total_ = static_cast<std::int64_t>(read.size());
        view_ = zengine::introspection::project_loaded(read, loaded_.rows, loaded_.columns,
            static_cast<std::size_t>(loaded_origin_));
        loaded_origin_ = std::min(loaded_origin_, loaded_total_ - static_cast<std::int64_t>(view_.shown.size()));
        zengine::introspection::mark_selected(view_, selected_, loaded_.columns);
        say_rows(mail, kLoadedPane, view_.rows, loaded_parts());
    }

    /// The host's answer about its arrangement. `answers_ask()` first -- provenance the bus
    /// attaches and no payload can write, stronger than the Manager's relay allows -- then the
    /// correlation, which says which grant is answered, so a late answer to an older room is
    /// dropped. Spent and dropped: nothing retains a row.
    void on(const ResolvedArrangement& said, loom::Mail& mail) {
        if (!answering(mail, arrangement_)) {
            return;
        }
        ++state_.readings;
        say_rows(mail, kArrangementPane,
                 intro::project_arrangement(said, arrangement_.rows, arrangement_.columns));
    }

    /// The same answer in version 2, sent to an office that accepts it: every row's state, and
    /// for a row that is not running its reason and the next step.
    void on(const zengine::workshop::v2::ResolvedArrangement& said, loom::Mail& mail) {
        if (!answering(mail, arrangement_)) {
            return;
        }
        ++state_.readings;
        say_rows(mail, kArrangementPane,
                 intro::project_arrangement(said, arrangement_.rows, arrangement_.columns));
    }

    /// The door's answer to this pane's search, checked as the arrangement's is, so an answer to
    /// a search the weaver has typed past is dropped. This subject moves (an overlay changes a
    /// power's contribution) and nothing here is told; the next grant or search asks again. The
    /// answer is retained for the cursor between asks: replaced whole, dropped at the next grant,
    /// never consulted for what is true now. A refusal is the door's sentence, shown as the list.
    void on(const PowersFound& said, loom::Mail& mail) {
        if (!answering(mail, powers_)) {
            return;
        }
        ++state_.readings;
        powers_ui_.reading = said;
        powers_ui_.read = true;
        say_powers(mail);
    }

    /// What the door said of the selected identity: its row and its whole contribution stack, and
    /// the one answer that may clear a selection -- the identity is gone (`take_described`). The
    /// answer is about the identity this pane asked of, matched by `answers_ask()` and the
    /// correlation, so a describe the weaver has moved past cannot relabel another selection.
    void on(const PowerDescribed& said, loom::Mail& mail) {
        if (!describing_.awaiting || !mail.answers_ask() ||
            mail.correlation() != describing_.pending) {
            return;
        }
        describing_.awaiting = false;
        PowerDescribed taken = said;
        taken.identity = describing_.identity;
        intro::take_described(powers_ui_, std::move(taken));
        say_powers(mail);
    }

    /// A key while Powers holds the keyboard, the pane checked before any state moves since this
    /// office offers three. Only the query field's editing vocabulary acts here; what the pane
    /// commands arrives as a resolved id (`PaneActionRequested`). A copy is said once; a paste is
    /// asked of the Skin, since only an owner can read the clipboard's current value.
    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        if (zengine::workshop::pane_escape::answer(key, mail, kIntrospectionRole,
                                                   [&] { return drop_selection(key.pane, mail); })) {
            return;
        }
        if (key.pane != kPowersPane) {
            return;
        }
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        const std::string searched = powers_ui_.query.text();
        if (!powers_ui_.query.consume(key.scancode, key.modifiers, clip_)) {
            return; // a key this pane's field has no word for changes nothing, says nothing
        }
        if (clip_.writes != copied_before) {
            mail.publish(surface::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_clipboard_paste(mail);
        }
        if (powers_ui_.query.text() != searched) {
            ask_powers(mail);
        }
        say_powers(mail);
    }

    /// Escape's default: the row a weaver chose in Loaded or Powers is let go, and the pane
    /// repainted; false when there was none. Like any clearing it publishes nothing.
    bool drop_selection(const std::string& pane, loom::Mail& mail) {
        if (pane == kLoadedPane && !selected_.empty()) {
            selected_.clear();
            zengine::introspection::mark_selected(view_, selected_, loaded_.columns);
            say_rows(mail, kLoadedPane, view_.rows, loaded_parts());
            return true;
        }
        if (pane == kPowersPane && !powers_ui_.selected().empty()) {
            powers_ui_.select(std::string());
            say_powers(mail);
            return true;
        }
        return false;
    }

    /// One of this pane's declared actions, by its resolved id: Workshop matched the keystroke
    /// against the weaver's keymap, so nothing here knows the key. Guarded by pane, as a key is.
    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        if (asked.pane != kPowersPane) {
            return;
        }
        if (asked.id == kPowersActionView) {
            powers_ui_.view = powers_ui_.view == intro::powers_view::kSources
                                  ? intro::powers_view::kOperators
                                  : intro::powers_view::kSources;
            ask_powers(mail);
            ask_described(mail);
        } else if (asked.id == kPowersActionUp || asked.id == kPowersActionDown) {
            const std::string was = powers_ui_.selected();
            intro::move_cursor(powers_ui_, asked.id == kPowersActionUp ? -1 : +1);
            if (powers_ui_.selected() != was) {
                ask_described(mail);
            }
        } else if (asked.id == kPowersActionSample) {
            // The one evaluation gesture; `sampleable` is empty for anything but a selected
            // Source, so the Operators view has no invocation path.
            ask_sample(mail);
            return;
        } else {
            return; // an id this pane never declared: Workshop sends none, and none acts
        }
        say_powers(mail);
    }

    // Scrolling Loaded changes its viewport, never its selected identity. Re-read
    // the owner instead of retaining an independently authoritative population.
    void wheel(const std::string& pane, double dy, loom::Mail& mail) {
        if (pane == kLoadedPane) {
            if (loaded_.awaiting || !std::isfinite(dy)) return;
            loaded_wheel_ += std::clamp(dy, -1000.0, 1000.0);
            const auto steps = static_cast<std::int64_t>(loaded_wheel_);
            loaded_wheel_ -= static_cast<double>(steps);
            const auto next = std::clamp(loaded_origin_ - steps, std::int64_t{0},
                                         std::max(std::int64_t{0}, loaded_total_ - 1));
            if (next == loaded_origin_) return;
            loaded_origin_ = next;
            ask(mail, loaded_, PaneRoom{kLoadedPane, loaded_.rows, loaded_.columns},
                loom::kManagerRole, loom::ListLoaded{});
            return;
        }
        if (pane != kPowersPane) {
            return;
        }
        powers_wheel_ += dy;
        const std::int64_t rows = static_cast<std::int64_t>(powers_wheel_);
        if (rows == 0) {
            return;
        }
        powers_wheel_ -= static_cast<double>(rows);
        const std::string was = powers_ui_.selected();
        intro::move_cursor(powers_ui_, -rows);
        if (powers_ui_.selected() == was) {
            return; // at the edge, or nothing to walk: nothing moved, nothing is re-said
        }
        ask_described(mail);
        say_powers(mail);
    }

    /// Typed characters: this pane's one field, so printable text is the query. Refused whole at
    /// this door unless every byte is printable ASCII: Workshop refuses a whole `PaneContent` for
    /// one byte a canvas cannot draw. (The Composer has the same exposure; another owner's.)
    void on(const PaneTextInput& typed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        if (typed.pane != kPowersPane || typed.text.empty() || !admissible(typed.text)) {
            return;
        }
        powers_ui_.query.type(typed.text);
        ask_powers(mail);
        say_powers(mail);
    }

    /// A copy said anywhere in the process, mirrored so a Terminal copy pastes into the query and
    /// gated as typing is. `writes` counts only this pane's copies, so nothing echoes.
    void on(const surface::ClipboardCopy& said, loom::Mail&) {
        if (!admissible(said.text)) {
            return;
        }
        clip_.text = said.text;
    }

    /// The Skin's answer to a paste this pane asked for, the one road foreign clipboard text has
    /// in: `answers_ask()` and the book's settlement, then the draft that asked must still be
    /// there (`draft_epoch`).
    void on(const surface::ClipboardText& a, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        const std::optional<loom::PendingAsk> settled =
            clip_asks_.settle(mail.correlation(), mail.sender());
        if (!settled || !pasting_ || settled->id != paste_ask_) {
            return;
        }
        pasting_ = false;
        if (powers_ui_.query.draft_epoch() != paste_epoch_) {
            return; // the query left the state that asked; the payload is discarded
        }
        if (a.readable) {
            if (!admissible(a.text)) {
                return; // refused WHOLE, at this pane's door, for `on(PaneTextInput)`'s reason
            }
            clip_.text = a.text; // the platform's current truth, asked for by this paste
        }
        const std::string searched = powers_ui_.query.text();
        powers_ui_.query.paste(clip_);
        if (powers_ui_.query.text() != searched) {
            ask_powers(mail);
        }
        say_powers(mail);
    }

    /// What a Source said when this weaver asked: `answers_ask()` and the correlation, as for the
    /// arrangement, so a newer ask drops an older answer. The identity kept is the one this pane
    /// asked for, never the payload's. Retained as history and never refreshed; its row leads with
    /// `sampled when asked` so the tense cannot be cut off.
    void on(const SourceSampled& said, loom::Mail& mail) {
        if (!sampling_.awaiting || !mail.answers_ask() ||
            mail.correlation() != sampling_.pending) {
            return;
        }
        sampling_.awaiting = false;
        powers_ui_.sample.present = true;
        powers_ui_.sample.identity = sampling_.identity;
        powers_ui_.sample.ok = said.ok;
        powers_ui_.sample.reason = said.reason;
        powers_ui_.sample.lines = said.lines;
        say_powers(mail);
    }

    /// An owner declining to answer, or an ask that reached nobody (a host with no arrangement or
    /// sample door is a real arrangement). The question is retired and nothing is said: a
    /// sentence about this tool's plumbing does not belong where a weaver reads facts. One counter
    /// mints every correlation, so at most one question matches.
    void on(const loom::Refused&, loom::Mail& mail) {
        for (Asked* q : {&loaded_, &arrangement_, &powers_}) {
            if (q->awaiting && mail.correlation() == q->pending) {
                q->awaiting = false;
            }
        }
        if (sampling_.awaiting && mail.correlation() == sampling_.pending) {
            sampling_.awaiting = false;
        }
        if (describing_.awaiting && mail.correlation() == describing_.pending) {
            describing_.awaiting = false;
        }
    }

private:
    /// One room and one outstanding question for one pane; none of it state, since the grant is
    /// re-sent whenever a valid offer refreshes the pane.
    struct Asked {
        std::int64_t rows = 0;
        std::int64_t columns = 0;
        std::uint64_t pending = 0; ///< the outstanding question for this pane, if any
        bool awaiting = false;
    };

    /// Every pane this office has, offered directly to Workshop, one offer per pane, so an offer
    /// this build gets wrong costs one pane.
    void announce(loom::Mail& mail) {
        offer(mail, PaneOffered{kLoadedPane, kLoadedPaneName, kLoadedPaneSummary, 7, 54});
        offer(mail, PaneOffered{kArrangementPane, kArrangementPaneName,
                                kArrangementPaneSummary, 9, 64});
        offer(mail, PaneOffered{kPowersPane, kPowersPaneName, kPowersPaneSummary, 8, 58});
        // ...and the Powers pane's four actions with their shipped defaults (`input::scan` and
        // `input::mod` numbers); applying a weaver's keymap is Workshop's.
        PaneActions rows;
        rows.pane = kPowersPane;
        rows.rows.push_back(
            PaneActionRow{kPowersActionView, "switch view", input::scan::kTab, input::mod::kNone});
        rows.rows.push_back(
            PaneActionRow{kPowersActionUp, "row up", input::scan::kUp, input::mod::kNone});
        rows.rows.push_back(
            PaneActionRow{kPowersActionDown, "row down", input::scan::kDown, input::mod::kNone});
        rows.rows.push_back(PaneActionRow{kPowersActionSample, "sample", input::scan::kReturn,
                                          input::mod::kNone});
        (void)mail.as_role(kIntrospectionRole).send_to_role(kWorkshopRole, rows);
    }

    void offer(loom::Mail& mail, const PaneOffered& said) {
        ++state_.offers;
        (void)mail.as_role(kIntrospectionRole).send_to_role(kWorkshopRole, said);
    }

    /// Ask one pane's owner its question, by role (the addresses that survive their holders),
    /// remembering the room it is for. One question per pane: a newer grant replaces the
    /// correlation, and the panes share the counter, never its values. As this office, because
    /// the arrangement door answers offices (workshop/arrangement.hpp).
    template <class Question>
    void ask(loom::Mail& mail, Asked& pane, const PaneRoom& room, const char* owner,
             const Question& question) {
        pane.rows = room.rows;
        pane.columns = room.columns;
        pane.pending = ++asked_;
        pane.awaiting = true;
        (void)mail.as_role(kIntrospectionRole).send_to_role(owner, question, pane.pending);
    }

    /// Is this the answer the pane waits on? `answers_ask()` first, which cannot be forged, then
    /// the correlation for which grant.
    bool answering(loom::Mail& mail, Asked& pane) {
        if (!pane.awaiting || !mail.answers_ask() || mail.correlation() != pane.pending) {
            return false;
        }
        pane.awaiting = false;
        return true;
    }

    /// ONE PANE'S OWN CANVAS: the room it holds there and the numbers its pictures take, and the
    /// prose room a host granting no canvas gives it. Transient, as every room is.
    struct Canvas {
        zengine::workshop::PaneCanvasRoom room;
        zengine::workshop::CanvasPictures pictures;
        /// What a press on the pane's picture means, spelled whole, and its number, which moves
        /// exactly when the spelling does.
        std::string press_map;
        std::int64_t meaning = 0;
        std::int64_t prose_rows = 0, prose_columns = 0;
        bool on() const { return room.grant > 0 && room.width > 0 && room.height > 0; }
    };

    Canvas* canvas_of(std::string_view pane) {
        if (pane == kLoadedPane) return &loaded_canvas_;
        if (pane == kArrangementPane) return &arrangement_canvas_;
        if (pane == kPowersPane) return &powers_canvas_;
        return nullptr;
    }

    /// Say what one pane shows, the one place content leaves: its picture while it holds a canvas
    /// room, its rows and caret as prose to a host granting none -- as this office, because
    /// Workshop drops personal speech from a weave that merely holds it (MSG-07).
    void say_rows(loom::Mail& mail, const char* pane,
                  std::vector<surface::SurfaceTextRow> rows,
                  std::vector<zengine::workshop::PaneRowPart> parts = {},
                  const zengine::workshop::RowsCaret& caret = {}) {
        if (Canvas* canvas = canvas_of(pane); canvas != nullptr && canvas->on()) {
            (void)mail.as_role(kIntrospectionRole)
                .send_to_role(kWorkshopRole,
                              zengine::workshop::rows_picture(
                                  canvas->room,
                                  canvas->pictures.next(canvas->room, meaning_of(*canvas, pane)),
                                  rows, parts, caret));
            return;
        }
        zengine::workshop::v4::PaneContent said;
        said.pane = pane;
        said.rows = std::move(rows);
        said.parts = std::move(parts);
        (void)mail.as_role(kIntrospectionRole).send_to_role(kWorkshopRole, said);
        // Powers says its caret every time, "none" included, so a caret the rows lost is gone.
        if (caret.row != surface::kNoCaret || std::string_view{pane} == kPowersPane) {
            (void)mail.as_role(kIntrospectionRole)
                .send_to_role(kWorkshopRole,
                              zengine::workshop::PaneCaret{pane, caret.row, caret.column});
        }
    }

    /// THE NUMBER OF WHAT A PRESS ON `pane` MEANS NOW: Loaded's rows by the weave each names,
    /// Powers' places by the control or power each is; Project's presses mean nothing. A repaint
    /// that moves no meaning -- a mark, a caret, a query typed -- keeps the number (`press_meaning`).
    std::int64_t meaning_of(Canvas& canvas, std::string_view pane) {
        std::string spelled;
        if (pane == kLoadedPane) {
            spelled = zengine::introspection::press_meaning(view_);
        } else if (pane == kPowersPane) {
            spelled = intro::press_meaning(powers_shown_);
        }
        if (canvas.meaning == 0 || spelled != canvas.press_map) {
            canvas.press_map = std::move(spelled);
            ++canvas.meaning;
        }
        return canvas.meaning;
    }

    /// WHAT LOADED CALLS ITS ROWS: a loaded weave's row by its name, `weave:<name>`.
    std::vector<zengine::workshop::PaneRowPart> loaded_parts() const {
        zengine::workshop::PartNames<zengine::workshop::PaneRowPart> named;
        for (std::size_t row = 0; row < view_.rows.size(); ++row) {
            if (const LoadedWeave* entry =
                    zengine::introspection::entry_at_row(view_, static_cast<std::int64_t>(row))) {
                (void)named.add(zengine::workshop::PaneRowPart{
                    "weave:" + entry->name, static_cast<std::int64_t>(row), 0, loaded_.columns});
            }
        }
        return named.take();
    }

    /// WHAT POWERS CALLS ITS PARTS: a power's row by its identity, `power:<identity>`, and its
    /// controls `control:sources`, `control:operators`, `control:composite` and `control:sample`.
    std::vector<zengine::workshop::PaneRowPart> powers_parts() const {
        zengine::workshop::PartNames<zengine::workshop::PaneRowPart> named;
        for (const intro::PowersSpan& span : powers_shown_.spans) {
            std::string name;
            switch (span.control) {
            case intro::powers_control::kSources: name = "control:sources"; break;
            case intro::powers_control::kOperators: name = "control:operators"; break;
            case intro::powers_control::kComposite: name = "control:composite"; break;
            case intro::powers_control::kSample: name = "control:sample"; break;
            case intro::powers_control::kEntry: name = "power:" + span.identity; break;
            default: break;
            }
            (void)named.add(zengine::workshop::PaneRowPart{std::move(name), span.row, span.first,
                                                           span.last - span.first + 1});
        }
        return named.take();
    }

    // ---- the Powers pane's own interaction --------------------------------------------

    /// Is every byte printable ASCII, the external row contract? The one door every road into
    /// the query passes: typed text, a mirrored copy, a paste.
    static bool admissible(std::string_view text) {
        for (const char c : text) {
            const unsigned char byte = static_cast<unsigned char>(c);
            if (byte < 0x20u || byte >= 0x7Fu) {
                return false;
            }
        }
        return true;
    }

    /// Say what Powers shows, the one place its projection is rebuilt: the caret's window is
    /// reconciled first against the capacity the chrome row cuts the query with. Silent with no
    /// room in force, or between a grant and its answer.
    void say_powers(loom::Mail& mail) {
        if (!powers_ui_.read || powers_.rows <= 0 || powers_.columns <= 0) {
            return;
        }
        powers_ui_.query.keep_caret_visible(intro::query_capacity(powers_ui_, powers_.columns));
        powers_shown_ = intro::project_powers_ui(powers_ui_, powers_.rows, powers_.columns);
        zengine::workshop::RowsCaret caret;
        caret.row = powers_shown_.caret_row;
        caret.column = powers_shown_.caret_col;
        say_rows(mail, kPowersPane, powers_shown_.rows, powers_parts(), caret);
    }

    /// A press in Powers, resolved against what is on screen through the spans the projection
    /// returned. A row selects and never samples, so a cold pane's first press is one act; a
    /// press that changes nothing republishes nothing.
    void on_powers_press(std::int64_t pressed_row, std::int64_t pressed_column, loom::Mail& mail) {
        const intro::PowersTarget hit =
            intro::target_at(powers_shown_, pressed_row, pressed_column);
        switch (hit.control) {
        case intro::powers_control::kSources:
        case intro::powers_control::kOperators: {
            const std::int64_t want = hit.control == intro::powers_control::kSources
                                          ? intro::powers_view::kSources
                                          : intro::powers_view::kOperators;
            if (powers_ui_.view == want) {
                return;
            }
            powers_ui_.view = want;
            ask_powers(mail);
            ask_described(mail);
            break;
        }
        case intro::powers_control::kComposite:
            powers_ui_.composite_only = !powers_ui_.composite_only;
            ask_powers(mail);
            break;
        case intro::powers_control::kEntry:
            // A press on a power may become a drag carrying its reference: held until the hand
            // moves, so a click that never moved asks nothing.
            carry_ = Carry{};
            carry_.armed = true;
            carry_.row = pressed_row;
            carry_.column = pressed_column;
            carry_.gesture = mail.correlation();
            carry_.identity = hit.identity;
            if (powers_ui_.selected() == hit.identity) {
                return;
            }
            powers_ui_.select(hit.identity);
            ask_described(mail);
            break;
        case intro::powers_control::kSample:
            ask_sample(mail);
            return;
        default:
            // Anything else selects nothing and clears nothing: a miss is not a deselection.
            return;
        }
        say_powers(mail);
    }

    /// THE HAND MOVED WITH THE BUTTON DOWN after a press on a power, onto another cell: its
    /// reference -- the identity and the two content ids the door's row carried -- is acquired
    /// under that press as a typed value, `zengine.OperatorRef`, for Workshop to carry. A form is
    /// no operator and carries none.
    void powers_drag(std::int64_t dragged_row, std::int64_t dragged_column, loom::Mail& mail) {
        if (!carry_.armed || carry_.started ||
            (dragged_row == carry_.row && dragged_column == carry_.column)) {
            return;
        }
        carry_.started = true;
        const zengine::workshop::PowerRow* row = nullptr;
        for (const zengine::workshop::PowerRow& r : powers_ui_.reading.rows) {
            if (r.identity == carry_.identity) {
                row = &r;
            }
        }
        if (row == nullptr || row->kind == zengine::workshop::kFormKind) {
            return;
        }
        const zengine::op::OperatorRef ref{row->identity,
                                           static_cast<loom::ContentId>(row->inputs.content_id),
                                           static_cast<loom::ContentId>(row->outputs.content_id)};
        const std::string bytes =
            zengine::inventory::encode_pair(zengine::op::encode_reference(ref), {});
        carry_.bytes.assign(bytes.begin(), bytes.end());
        carry_.ask = ++asked_;
        (void)mail.as_role(kIntrospectionRole)
            .send_to_role(kWorkshopRole,
                          zengine::workshop::PaneOperationRequested{
                              kPowersPane, kWorkshopRole,
                              zengine::workshop::PaneValueCarryRequested::zen_name, 1,
                              static_cast<std::int64_t>(carry_.gesture)},
                          carry_.ask);
    }

public:
    /// Workshop's answer to that acquisition: allowed, the reference is handed over to carry under
    /// the press's own number; refused, nothing is carried.
    void on(const zengine::workshop::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask() || carry_.ask == 0 || mail.correlation() != carry_.ask) {
            return;
        }
        carry_.ask = 0;
        if (!answer.allowed) {
            return;
        }
        (void)mail.as_role(kIntrospectionRole)
            .send_to_role(kWorkshopRole,
                          zengine::workshop::PaneValueCarryRequested{kPowersPane, carry_.identity,
                                                                     carry_.bytes, true},
                          carry_.gesture);
    }

    void on(const zengine::workshop::PaneCarryAnswered&, loom::Mail&) {
        carry_ = Carry{}; // carried or refused, the press is spent
    }

private:
    /// Ask the host to run one Source because a weaver said so: the only thing here that can
    /// cause an evaluation, called only from `Return` and `[ Sample ]`. One outstanding: a second
    /// gesture replaces the correlation, and the door still spends both.
    void ask_sample(loom::Mail& mail) {
        std::string identity = intro::sampleable(powers_ui_);
        if (identity.empty()) {
            return; // nothing selected, or the selection is an Operator: no gesture exists
        }
        ++state_.samples;
        sampling_.pending = ++asked_;
        sampling_.awaiting = true;
        sampling_.identity = identity;
        (void)mail.as_role(kIntrospectionRole)
            .send_to_role(kSampleRole, SampleRequested{std::move(identity)}, sampling_.pending);
    }

    /// Ask the door this pane's search, as this office: a newer search replaces the correlation,
    /// so the answer to one the weaver has typed past is dropped. `FindPowers` is a value built
    /// at its own schema, its fields optional, so it leaves as that value.
    void ask_powers(loom::Mail& mail) {
        powers_.pending = ++asked_;
        powers_.awaiting = true;
        (void)mail.bus().office_send_to_role(
            kIntrospectionRole, kPowersRole,
            loom::Message(zengine::workshop::find_powers_value(intro::powers_question(powers_ui_)),
                          self_, loom::WeaveId{}, powers_.pending));
    }

    /// Ask the door about the selected identity: its row and every contribution eligible to
    /// satisfy it. With nothing selected, nothing is asked.
    void ask_described(loom::Mail& mail) {
        const std::string& identity = powers_ui_.selected();
        if (identity.empty()) {
            return;
        }
        describing_.pending = ++asked_;
        describing_.awaiting = true;
        describing_.identity = identity;
        (void)mail.as_role(kIntrospectionRole)
            .send_to_role(kPowersRole, DescribePower{identity}, describing_.pending);
    }

    /// Ask the Skin what the platform clipboard holds, for a paste the query requested; the
    /// epoch says the draft the answer is for is still in the box.
    void begin_clipboard_paste(loom::Mail& mail) {
        const loom::AskOpened opened = clip_asks_.open_to_role(
            surface::kSkinRole, surface::ClipboardTextRequested::zen_name,
            surface::ClipboardTextRequested::zen_version);
        if (!opened) {
            return;
        }
        paste_ask_ = opened.id;
        paste_epoch_ = powers_ui_.query.draft_epoch();
        pasting_ = true;
        (void)mail.as_role(kIntrospectionRole)
            .send_to_role(surface::kSkinRole, surface::ClipboardTextRequested{},
                          opened.correlation);
    }

    zengine::ActivationCursor activation_;
    std::uint64_t asked_ = 0; ///< this incarnation's correlation counter, for all three panes
    Asked loaded_;
    std::int64_t loaded_origin_ = 0, loaded_total_ = 0;
    double loaded_wheel_ = 0;
    Asked arrangement_;
    Asked powers_;
    Canvas loaded_canvas_, arrangement_canvas_, powers_canvas_;
    /// What Loaded is showing, and its rows' map back to entries: the presentation, bounded by
    /// the room and emptied at every grant, never an inventory of what is loaded.
    LoadedView view_;
    /// The selected entry, as the library name the row showed -- a name survives a resize, a row
    /// does not. Transient: not state, in no file, and nothing else in the process has one.
    std::string selected_;

    // ---- what the Powers pane knows, and what it is showing ---------------------------

    /// The weaver's side of Powers (introspection/powers.hpp states each member's law), all
    /// transient; the reading is the host's only words in here, replaced whole at every grant.
    intro::PowersUi powers_ui_;
    /// Fractional wheel notches over Powers not yet worth a row; transient like the cursor.
    double powers_wheel_ = 0.0;

    /// What Powers shows and what its places mean: presentation, emptied at every grant.
    intro::PowersView powers_shown_;

    /// THE ONE OUTSTANDING SAMPLE, and the identity it was asked for. It is not an
    /// `Asked`: a sample belongs to no room, and giving it rows and columns it would
    /// never spend would invite one of them to be read.
    struct Sampling {
        std::uint64_t pending = 0;
        bool awaiting = false;
        std::string identity;
    };
    Sampling sampling_;

    /// THE ONE OUTSTANDING DESCRIBE, and the identity it asked about: its answer is kept as the
    /// description of that identity, never of whatever is selected when it lands.
    struct Describing {
        std::uint64_t pending = 0;
        bool awaiting = false;
        std::string identity;
    };
    Describing describing_;

    /// A press on a power that a drag may yet carry: where it was, under which input, which power,
    /// and once the hand moved, the acquisition asked and the reference's bytes.
    struct Carry {
        bool armed = false, started = false;
        std::int64_t row = 0, column = 0;
        std::uint64_t gesture = 0, ask = 0;
        std::string identity;
        loom::Bytes bytes;
    };
    Carry carry_;

    /// THE PROCESS'S COPIED TEXT AS THIS PANE LAST HEARD IT, and the book for the one
    /// paste conversation it can hold open. Two, because a clipboard is a mirror and
    /// an ask is a question -- the Composer's shipped pair, for its reasons.
    component::Clipboard clip_;
    loom::AskBook clip_asks_{1};
    std::uint64_t paste_ask_ = 0;
    std::uint64_t paste_epoch_ = 0;
    bool pasting_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(IntrospectionWeave)
