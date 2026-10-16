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
// Reference: introspection/docs/zengine.introspection.md, the manual it carries and answers.

#include "seats.hpp"
#include "vocabulary.hpp"

#include "activation/activation.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/arrangement_vocabulary.hpp"
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_document.hpp"
#include "workshop/pane_escape.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/powers_vocabulary.hpp"
#include "workshop/sample_vocabulary.hpp"

#include "manuals/zengine.introspection.hpp" // this weave's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace intro = zengine::introspection;
using intro::kArrangementPane;
using intro::kIntrospectionRole;
using intro::kLoadedPane;
using intro::kPowersPane;
using intro::kWorkshopRole;

/// What this provider has done, and it is all counters: the loaded map, the resolved rows and
/// the contribution stacks belong to their owners, and a copy here would be the answer that goes
/// stale. The selection and the Powers pane's interaction are the seats', never snapshotted: a
/// revived incarnation has no room, and nothing a selection could be of.
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
          loom::Accept<loom::Activated, ws::PaneCatalogRequested, ws::PaneRoom, ws::PaneCanvasRoom,
                       ws::PaneCanvasPointer, ws::PaneCanvasRejected, ws::PaneKey,
                       ws::PaneTextInput, ws::PaneActionRequested, loom::Result,
                       ws::PaneOperationAnswered, ws::PaneCarryAnswered, loom::Refused,
                       ws::ResolvedArrangement, ws::v2::ResolvedArrangement, ws::PowersFound,
                       ws::PowerDescribed, ws::SourceSampled, surface::ClipboardCopy,
                       surface::ClipboardText, ws::PaneDocumentRequested>,
          loom::Emit<ws::v2::PaneOffered, ws::v3::PaneActions, ws::v4::PaneContent,
                     ws::v5::PaneCanvasContent, ws::PaneCaret, ws::PanePassRequested,
                     intro::LoadedSelected, loom::ListLoaded, ws::ArrangementRequested,
                     ws::DescribePower, ws::SampleRequested, ws::PaneOperationRequested,
                     ws::PaneValueCarryRequested, ws::PaneEscapeUnspent, surface::ClipboardCopy,
                     surface::ClipboardTextRequested, ws::PaneDocumentShown>> {
public:
    /// First breath, only if Loom says so: `ActivationCursor` requires Loom's attestation and a
    /// sequence not yet acted on, so no weave can make a pane appear by sending `zen.Activated`.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (activation_.accept(mail, a)) {
            announce(mail);
        }
    }

    /// Workshop asking who has panes. `authored_from_role`, not `sender()`: the ask is a
    /// publication, and only Loom's stamp on its authorship says it was Workshop.
    void on(const ws::PaneCatalogRequested&, loom::Mail& mail) {
        if (workshop_said(mail)) {
            announce(mail);
        }
    }

    /// THE MANUAL OF A PANE THIS OFFICE OFFERS, to the asker alone: the one page this weave carries,
    /// answered for whichever of its three pane keys the ask names; any other key is refused.
    void on(const ws::PaneDocumentRequested& ask, loom::Mail& mail) {
        std::string_view pane = kLoadedPane;
        for (const char* offered : {kLoadedPane, kArrangementPane, kPowersPane}) {
            if (ask.pane == offered) {
                pane = offered;
            }
        }
        (void)mail.answer(
            ws::pane_document(ask, pane, zengine::manual::pages::zengine_introspection::kManual));
    }

    /// Workshop granting one pane its prose budget, spent only while the pane holds no canvas room.
    /// A forged room grants nothing; a room for a pane this office lacks is neither counted nor
    /// answered.
    void on(const ws::PaneRoom& room, loom::Mail& mail) {
        if (intro::PaneSeat* seat = workshop_said(mail) ? seat_of(room.pane) : nullptr) {
            grant(mail, seat->prose_room(room));
        }
    }

    /// The pane's own canvas: while it holds a room there it draws its rows as its picture, the
    /// lattice its budget. Every grant is a room grant (`grant`), the picture's numbers its own.
    void on(const ws::PaneCanvasRoom& room, loom::Mail& mail) {
        if (intro::PaneSeat* seat = workshop_said(mail) ? seat_of(room.pane) : nullptr) {
            grant(mail, seat->canvas_room(room));
        }
    }

    /// A refused picture leaves the last good one showing; the next reading draws again.
    void on(const ws::PaneCanvasRejected&, loom::Mail&) {}

    /// A HAND ON A PANE'S PICTURE, read back to the row and column of its lattice: a primary press
    /// is the press a row was, its motion a drag, the wheel the wheel; a right press is handed
    /// back, so Workshop's own pane menu opens where it was made. Only for the room the pane holds,
    /// and a primary press only on the picture drawn under the press map in force.
    void on(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
        namespace cp = ws::canvas_pointer;
        const intro::PaneSeat* seat = workshop_said(mail) ? seat_of(event.pane) : nullptr;
        const std::optional<ws::RowCell> at = seat == nullptr ? std::nullopt : seat->cell_of(event);
        if (!at) {
            return;
        }
        const bool powers = event.pane == kPowersPane;
        if (event.phase == cp::kWheel) {
            if (powers) powers_.wheel(speaking(mail), event.dy);
            if (event.pane == kLoadedPane) loaded_.wheel(speaking(mail), event.dy);
        } else if (event.phase == cp::kMove) {
            if (powers) powers_.drag(speaking(mail), at->row, at->column);
        } else if (event.phase == cp::kRelease || event.phase == cp::kLost) {
            if (powers) powers_.release();
        } else if (event.phase == cp::kPress && event.button == 3) {
            (void)ws::pane_menu::pass_back(mail, kIntrospectionRole, event.pane);
        } else if (event.phase == cp::kPress && event.button == 1 && at->shown &&
                   seat->shows(event)) {
            // `arrangement` is read-only: a press there gives it the keys and says nothing.
            if (powers && powers_.press(speaking(mail), at->row, at->column)) ++state_.samples;
            if (event.pane == kLoadedPane && loaded_.press(at->row, mail)) ++state_.selections;
        }
    }

    void on(const loom::Result& r, loom::Mail& mail) {
        if (loaded_.on_result(r, mail)) ++state_.readings;
    }

    void on(const ws::ResolvedArrangement& said, loom::Mail& mail) {
        if (arrangement_.on_resolved(said, mail)) ++state_.readings;
    }

    void on(const ws::v2::ResolvedArrangement& said, loom::Mail& mail) {
        if (arrangement_.on_resolved(said, mail)) ++state_.readings;
    }

    void on(const ws::PowersFound& said, loom::Mail& mail) {
        if (powers_.on_found(said, mail)) ++state_.readings;
    }

    void on(const ws::PowerDescribed& said, loom::Mail& mail) { powers_.on_described(said, mail); }

    void on(const ws::SourceSampled& said, loom::Mail& mail) { powers_.on_sampled(said, mail); }

    /// A key, the pane checked before any state moves since this office offers three. Escape
    /// lets go of a pane's selection; anything else is Powers' query field's.
    void on(const ws::PaneKey& key, loom::Mail& mail) {
        if (!workshop_said(mail) ||
            ws::pane_escape::answer(key, mail, kIntrospectionRole,
                                    [&] { return drop_selection(key.pane, mail); })) {
            return;
        }
        if (key.pane == kPowersPane) {
            powers_.on_key(speaking(mail), key);
        }
    }

    /// One of a pane's declared actions, by its resolved id, guarded by pane as a key is.
    void on(const ws::PaneActionRequested& asked, loom::Mail& mail) {
        if (workshop_said(mail) && asked.pane == kPowersPane &&
            powers_.on_action(speaking(mail), asked.id)) {
            ++state_.samples;
        }
    }

    /// Typed characters, the Powers query's.
    void on(const ws::PaneTextInput& typed, loom::Mail& mail) {
        if (workshop_said(mail) && typed.pane == kPowersPane) {
            powers_.on_typed(speaking(mail), typed.text);
        }
    }

    void on(const surface::ClipboardCopy& said, loom::Mail&) { powers_.on_copy(said); }

    void on(const surface::ClipboardText& a, loom::Mail& mail) {
        powers_.on_clipboard(speaking(mail), a);
    }

    void on(const ws::PaneOperationAnswered& answer, loom::Mail& mail) {
        powers_.on_approval(answer, mail);
    }

    void on(const ws::PaneCarryAnswered&, loom::Mail&) { powers_.carried(); }

    /// An owner declining to answer, or an ask that reached nobody (a host with no arrangement or
    /// sample door is a real arrangement). The question is retired and nothing is said: a sentence
    /// about this tool's plumbing does not belong where a weaver reads facts.
    void on(const loom::Refused&, loom::Mail& mail) {
        loaded_.retire(mail.correlation());
        arrangement_.retire(mail.correlation());
        powers_.refused(mail.correlation());
    }

private:
    /// Is this Workshop's own word? Anything else asks nothing and grants nothing, and is counted.
    bool workshop_said(const loom::Mail& mail) {
        if (mail.authored_from_role(kWorkshopRole)) {
            return true;
        }
        ++state_.refused;
        return false;
    }

    intro::PaneSeat* seat_of(std::string_view pane) {
        if (pane == kLoadedPane) return &loaded_;
        if (pane == kArrangementPane) return &arrangement_;
        if (pane == kPowersPane) return &powers_;
        return nullptr;
    }

    intro::Voice speaking(loom::Mail& mail) { return intro::Voice{mail, self_, asked_}; }

    /// WORKSHOP GRANTING ONE PANE ITS BUDGET, from either room. Each pane keeps its own room and
    /// outstanding question, or the last grant would decide how the others are drawn. The room is
    /// kept and the owner asked; content goes when the owner answers, never from a previous
    /// reading -- Workshop clears its cache before every grant, and its `(waiting for the
    /// provider)` is the honest gap. A seat that holds no room to grant grants nothing.
    void grant(loom::Mail& mail, const std::optional<ws::PaneRoom>& room) {
        if (!room) {
            return;
        }
        if (room->pane == kLoadedPane) {
            loaded_.grant(speaking(mail), *room);
        } else if (room->pane == kArrangementPane) {
            arrangement_.grant(speaking(mail), *room);
        } else if (room->pane == kPowersPane) {
            powers_.grant(speaking(mail), *room);
        } else {
            return;
        }
        ++state_.rooms;
    }

    /// Escape's default: the row a weaver chose in Loaded or Powers is let go, and the pane
    /// repainted; false when there was none.
    bool drop_selection(const std::string& pane, loom::Mail& mail) {
        if (pane == kLoadedPane) return loaded_.drop_selection(mail);
        if (pane == kPowersPane) return powers_.drop_selection(mail);
        return false;
    }

    /// Every pane this office has, offered directly to Workshop, one offer per pane, so an offer
    /// this build gets wrong costs one pane; then the actions the Powers pane answers to.
    void announce(loom::Mail& mail) {
        for (const ws::v2::PaneOffered& said :
             {loaded_.offering(), arrangement_.offering(), powers_.offering()}) {
            ++state_.offers;
            (void)mail.as_role(kIntrospectionRole).send_to_role(kWorkshopRole, said);
        }
        (void)mail.as_role(kIntrospectionRole).send_to_role(kWorkshopRole, powers_.actions());
    }

    zengine::ActivationCursor activation_;
    std::uint64_t asked_ = 0; ///< this incarnation's correlation counter, for all three panes
    intro::LoadedSeat loaded_;
    intro::ArrangementSeat arrangement_;
    intro::PowersSeat powers_;
};

} // namespace

ZEN_EXPORT_WEAVE(IntrospectionWeave)
