// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Introspection provider: a loadable weave offering Workshop three read-only panes about the
// running system, each with its own owner -- `loaded` (the Kernel's loaded() map), `arrangement`
// (the realization owner's plan and rows) and `powers` (the host's operator catalog). They
// disagree on purpose: a provider is a row of `arrangement` and absent from `loaded`. Every fact
// is asked for, spent and dropped; a room grant re-reads it, since nothing can be subscribed to.
// Pane law: agents/panes.md

// It cannot load, unload, mount or evaluate: its sends are the pane protocol, `zen.ListLoaded`
// (enumeration, not the load capability), `LoadedSelected`, the arrangement and powers questions,
// one `SampleRequested` per maker gesture, and the clipboard pair. It links no operator target,
// so browsing cannot evaluate. The loader binds `allow_any()` to every library, so this is a
// claim about what the weave does, not containment. Reference: docs/reference/introspection.md.

#include "loaded.hpp"
#include "powers.hpp"
#include "resolved.hpp"
#include "vocabulary.hpp"

#include "activation/activation.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/arrangement_vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"
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
using zengine::workshop::kArrangementRole;
using zengine::workshop::kSampleRole;
using zengine::workshop::PaneActionRequested;
using zengine::workshop::PaneActionRow;
using zengine::workshop::PaneActions;
using zengine::workshop::PaneCatalogRequested;
using zengine::workshop::PaneContent;
using zengine::workshop::PaneKey;
using zengine::workshop::v2::PaneOffered;
using zengine::workshop::PanePressed;
using zengine::workshop::PaneRoom;
using zengine::workshop::PaneTextInput;
using zengine::workshop::PaneWheel;
using zengine::workshop::PowersRequested;
using zengine::workshop::ResolvedArrangement;
using zengine::workshop::ResolvedPowers;
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
    std::int64_t selections = 0; ///< maker selections published as `LoadedSelected`
    std::int64_t samples = 0;    ///< explicit maker sample gestures this office asked for
    ZEN_EXPOSE();
    ZEN_SHAPE(IntrospectionState, 3, ZEN_FIELD(offers), ZEN_FIELD(rooms), ZEN_FIELD(readings),
              ZEN_FIELD(refused), ZEN_FIELD(selections), ZEN_FIELD(samples));
};

class IntrospectionWeave
    : public loom::WeaveBase<
          IntrospectionWeave, IntrospectionState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PanePressed, PaneKey,
                       PaneTextInput, PaneWheel, PaneActionRequested, loom::Result,
                       loom::Refused, ResolvedArrangement, zengine::workshop::v2::ResolvedArrangement,
                       ResolvedPowers, SourceSampled,
                       surface::ClipboardCopy, surface::ClipboardText>,
          loom::Emit<PaneOffered, PaneActions, PaneContent, LoadedSelected, loom::ListLoaded,
                     ArrangementRequested, PowersRequested, SampleRequested,
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

    /// Workshop granting one pane its prose budget. Each pane keeps its own room and outstanding
    /// question (`Asked`), or the last grant would decide how the others are drawn. The room is
    /// kept and the owner asked; content goes when the owner answers, never from a previous
    /// reading -- Workshop clears its cache before every grant, and its `(waiting for the
    /// provider)` is the honest gap. The row map goes with the old projection, so no press is
    /// read against rows no longer shown; the selected identity stays.
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return; // a forged room grants nothing and produces no content
        }
        if (room.pane == kLoadedPane) {
            ++state_.rooms;
            view_ = LoadedView{};
            ask(mail, loaded_, room, loom::kManagerRole, loom::ListLoaded{});
        } else if (room.pane == kArrangementPane) {
            ++state_.rooms;
            ask(mail, arrangement_, room, kArrangementRole, ArrangementRequested{});
        } else if (room.pane == kPowersPane) {
            ++state_.rooms;
            // The reading goes with the projection: until the host answers there is nothing to
            // derive a view from or read a press against. What the maker authored survives --
            // the view, the query, the filter, both selections and the retained sample.
            powers_ui_.reading = ResolvedPowers{};
            powers_ui_.read = false;
            powers_shown_ = intro::PowersView{};
            ask(mail, powers_, room, kArrangementRole, PowersRequested{});
        }
        // A room for a pane this provider does not have is neither counted nor answered:
        // Workshop grants rooms only for offers it admitted.
    }

    /// A maker pressed a row: a gesture becomes a fact, read against the projection on screen
    /// with nothing re-asked -- re-reading the Manager here could select something the maker was
    /// never shown. A row naming no entry selects nothing and clears nothing. The same row pressed
    /// twice publishes twice (a selection is an occurrence) and re-sends no picture.
    void on(const PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return; // a forged press selects nothing and publishes nothing
        }
        if (press.pane == kPowersPane) {
            on_powers_press(press, mail);
            return;
        }
        if (press.pane != kLoadedPane) {
            // `arrangement` is read-only: a press there is consumed by Workshop and says nothing.
            return;
        }
        const LoadedWeave* entry = zengine::introspection::entry_at_row(view_, press.row);
        if (entry == nullptr) {
            return; // a heading, a note, an omission marker, a blank, or no projection at all
        }
        const bool moved = selected_ != entry->name;
        selected_ = entry->name;
        const std::string role = entry->role;
        if (moved) {
            zengine::introspection::mark_selected(view_, selected_, loaded_.columns);
            say_rows(mail, kLoadedPane, view_.rows);
        }
        ++state_.selections;
        // Published, not addressed: who ought to care is not this pane's decision. As this
        // office, because a fact about a maker's gesture is worth what its office is.
        (void)mail.as_role(kIntrospectionRole)
            .publish(LoadedSelected{kLoadedPane, selected_, role});
    }

    /// The Manager's answer, matched on the correlation this weave minted and an open question --
    /// all an asker can check, since the Manager relays personally: no authored role, no expected
    /// sender. A weave able to send `zen.Result` with a live private correlation could supply
    /// rows; that is the process tier's problem, not a claim this seam makes. A selection is
    /// cleared only here, when a reading's whole population lacks it, and clearing publishes
    /// nothing: a departure is not a maker's gesture.
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
        say_rows(mail, kLoadedPane, view_.rows);
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

    /// The host's answer about its powers, checked as the arrangement's is. This subject moves (an
    /// overlay changes a power's contribution) and nothing here is told; the next grant asks
    /// again. The answer is retained for search, filter and cursor between grants: a snapshot
    /// replaced whole, dropped at the next grant, never consulted for what is true now. A
    /// selection is cleared only when a reading's population lacks it (`revalidate`).
    void on(const ResolvedPowers& said, loom::Mail& mail) {
        if (!answering(mail, powers_)) {
            return;
        }
        ++state_.readings;
        powers_ui_.reading = said;
        powers_ui_.read = true;
        intro::revalidate(powers_ui_);
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
        if (key.pane != kPowersPane) {
            return;
        }
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (!powers_ui_.query.consume(key.scancode, key.modifiers, clip_)) {
            return; // a key this pane's field has no word for changes nothing, says nothing
        }
        if (clip_.writes != copied_before) {
            mail.publish(surface::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_clipboard_paste(mail);
        }
        say_powers(mail);
    }

    /// One of this pane's declared actions, by its resolved id: Workshop matched the keystroke
    /// against the maker's keymap, so nothing here knows the key. Guarded by pane, as a key is.
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
        } else if (asked.id == kPowersActionUp) {
            intro::move_cursor(powers_ui_, -1);
        } else if (asked.id == kPowersActionDown) {
            intro::move_cursor(powers_ui_, +1);
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
    void on(const PaneWheel& wheel, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        if (wheel.pane == kLoadedPane) {
            if (loaded_.awaiting || !std::isfinite(wheel.dy)) return;
            loaded_wheel_ += std::clamp(wheel.dy, -1000.0, 1000.0);
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
        if (wheel.pane != kPowersPane) {
            return;
        }
        powers_wheel_ += wheel.dy;
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
        powers_ui_.query.paste(clip_);
        say_powers(mail);
    }

    /// What a Source said when this maker asked: `answers_ask()` and the correlation, as for the
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
    /// sentence about this tool's plumbing does not belong where a maker reads facts. One counter
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
        // `input::mod` numbers); applying a maker's keymap is Workshop's.
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

    /// Say what one pane shows, the one place content leaves: as this office, because Workshop
    /// drops personal speech from a weave that merely holds it (MSG-07).
    void say_rows(loom::Mail& mail, const char* pane,
                  std::vector<surface::SurfaceTextRow> rows) {
        PaneContent said;
        said.pane = pane;
        said.rows = std::move(rows);
        (void)mail.as_role(kIntrospectionRole).send_to_role(kWorkshopRole, said);
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
        say_rows(mail, kPowersPane, powers_shown_.rows);
    }

    /// A press in Powers, resolved against what is on screen through the spans the projection
    /// returned. A row selects and never samples, so a cold pane's first press is one act; a
    /// press that changes nothing republishes nothing.
    void on_powers_press(const PanePressed& press, loom::Mail& mail) {
        const intro::PowersTarget hit =
            intro::target_at(powers_shown_, press.row, press.column);
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
            break;
        }
        case intro::powers_control::kComposite:
            powers_ui_.composite_only = !powers_ui_.composite_only;
            break;
        case intro::powers_control::kEntry:
            if (powers_ui_.selected() == hit.identity) {
                return;
            }
            powers_ui_.select(hit.identity);
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

    /// Ask the host to run one Source because a maker said so: the only thing here that can
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
    /// What Loaded is showing, and its rows' map back to entries: the presentation, bounded by
    /// the room and emptied at every grant, never an inventory of what is loaded.
    LoadedView view_;
    /// The selected entry, as the library name the row showed -- a name survives a resize, a row
    /// does not. Transient: not state, in no file, and nothing else in the process has one.
    std::string selected_;

    // ---- what the Powers pane knows, and what it is showing ---------------------------

    /// The maker's side of Powers (introspection/powers.hpp states each member's law), all
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
