// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Info pane: a loadable weave that offers Workshop the properties of the one pane a weaver
// chose to inspect (`property_view.hpp`) -- plus independent typed value views
// (`value_view.hpp`). The seam has three parts: a picture the host derives and publishes when
// it changes (`PaneSubjectShown`), the one door that names the subject
// (`InspectPaneRequested`), and a commit naming the rows its draft was typed for
// (`PaneCommitRequested`); nothing here touches a desk or a definition.
// Workshop law: agents/workshop/info-body.md

// The subject is named by an office and nothing else moves it: the Pane Manager's `inspect in
// Info` row asks this pane, which asks the host. The host's one inventory (`PaneInventory`) says
// which of this office's views are on the desk. Each view draws its rows, and the caret of a line
// being typed, in its own area of the window.

#include "info/vocabulary.hpp"

#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/inspection_seam_vocabulary.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_document.hpp"
#include "workshop/pane_seat.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "property_view.hpp"
#include "value_view.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_escape.hpp"
#include "workshop/setup_control.hpp"
#include "component/text_box.hpp"
#include "input/vocabulary.hpp"
#include "graphics/vocabulary.hpp"

#include "manuals/zengine.info.hpp" // this weave's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <array>
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
namespace pane = zengine::info_pane;

using ws::InspectPaneRequested;
using ws::PaneActionRequested;
using ws::v4::PaneActionRow;
using ws::v4::PaneActions;
using ws::PaneCommitRequested;
using ws::PaneInventory;
using ws::PaneInventoryRequested;
using ws::PaneKey;
using ws::v2::PaneOffered;
using ws::PaneOperationRequested;
using ws::PaneOperationAnswered;
using ws::PaneSubjectActed;
using ws::PaneSubjectRequested;
using ws::PaneSubjectShown;
using ws::PaneTextInput;

using pane::kWorkshopRole;

// =============================================================================
// The weave
// =============================================================================

/// THE OFFICE OF THE FOUR VIEWS: its activation, its offers on Workshop's catalog request, and
/// the manual of any view it offers, the whole page the weave's.
using Office = ws::MultiPaneFacet<pane::kInfoPaneRole, pane::kInfoPane,
                                  zengine::manual::pages::zengine_info::kManual>;

class InfoPaneWeave
    : public zengine::With<Office>::Weave<
          InfoPaneWeave, pane::InfoPaneState,
          loom::Accept<ws::PaneCarryAnswered, ws::PaneResetRequested, ws::PaneCanvasDrop, ws::PaneCanvasValueDrop, PaneOperationAnswered, zengine::inventory::InventoryEntry,
                       InspectPaneRequested,
                       loom::Refused, PaneKey,
                       PaneTextInput, PaneActionRequested, PaneInventory, PaneSubjectShown,
                       PaneSubjectActed, ws::PaneSettingsAnswered, ws::PaneSettingCommitAnswered,
                       loom::DispatchRefused, gfx::ClipboardCopy,
                       gfx::ClipboardText, ws::PaneMenuAnswered, ws::PaneObservationAnswered, zengine::inventory::InventoryChanged,
                       loom::PokeStructure, ws::PaneLaunchAnswered, ws::PaneCloseAnswered>,
          loom::Emit<ws::PaneValueCarryRequested, PaneOperationRequested, zengine::inventory::InventoryRead, zengine::inventory::InventoryAdd,
                     zengine::inventory::InventoryWrite, PaneOffered, PaneActions, PaneInventoryRequested,
                     PaneSubjectRequested, InspectPaneRequested, PaneSubjectActed, PaneCommitRequested,
                     ws::PaneSettingsRequested, ws::PaneSettingCommitRequested,
                     gfx::ClipboardCopy, gfx::ClipboardTextRequested, ws::PanePassRequested,
                     ws::PaneMenuRequested, ws::PaneObservationRequested, ws::PaneObservationContinued,
                     ws::PaneEscapeUnspent,
                     ws::PaneObservationEnded, loom::PokeDescribe, ws::PaneLaunchRequested,
                     ws::PaneCloseRequested>> {
public:
    InfoPaneWeave() {
        for (std::size_t slot = 1; slot <= pane::kMaxInfoViews; ++slot) {
            views_.emplace_back(pane::view_key(slot), slot, state_);
            canvases_[slot - 1] = ws::PaneCanvasSeat(pane::view_key(slot), pane::kInfoPaneRole);
        }
        views_.front().allocate();
    }

    /// THE WEAVER'S POSITION AND EVERY ALLOCATED VIEW, as a reload in place carries them
    /// (`InfoPaneState`), read from the views as they stand.
    loom::Value snapshot() const override {
        pane::InfoPaneState saved = state_;
        saved.cursor = properties_.cursor();
        saved.views.clear();
        for (const auto& v : views_)
            if (auto kept = v.carried()) saved.views.push_back(std::move(*kept));
        return loom::to_value(saved);
    }
    /// ...and put back in this image before its first delivery: its activation offers each view
    /// again, and ends every lease the old image held. What stays in the state is each view's slot
    /// and open levels, where its folds read and write them.
    void revive(const loom::Value& v) override {
        state_ = loom::from_value<pane::InfoPaneState>(v);
        properties_.revive(state_.cursor);
        const std::vector<pane::InfoViewState> kept_views = state_.views;
        for (const auto& kept : kept_views)
            if (kept.slot >= 1 && static_cast<std::size_t>(kept.slot) <= views_.size())
                views_[static_cast<std::size_t>(kept.slot) - 1].carry_in(kept);
        std::vector<pane::InfoViewState> live;
        for (const auto& kept : state_.views) {
            if (kept.slot < 1 || static_cast<std::size_t>(kept.slot) > views_.size() ||
                !views_[static_cast<std::size_t>(kept.slot) - 1].allocated())
                continue;
            pane::InfoViewState row;
            row.slot = kept.slot;
            row.open = kept.open;
            live.push_back(std::move(row));
        }
        state_.views = std::move(live);
    }

    /// THE DEMO AND TEST RESET DOOR: the default view drops its drafts; a slot view is retired.
    /// Refused while any owner operation of that view is pending.
    void on(const ws::PaneResetRequested& request, loom::Mail& mail) {
        pane::ValueView* v = view_of(request.pane);
        if (!v || v->busy() || (v->is_default() && properties_.waiting())) {
            (void)mail.answer(loom::Refused{"Info reset needs its pane and no pending commit"}); return;
        }
        pane::ViewContext c{mail, asked_};
        const bool renamed = v->retire(c, {});
        if (v->is_default()) {
            properties_.reset(); paste_ = Paste{};
        } else {
            // A RESET SLOT IS A FRESH, EMPTY VIEW a setup may name: offered, allocated, no draft.
            v->allocate();
            if (renamed || !v->offered) offer(*v, mail);
        }
        declare(*v, mail); say(*v, mail); (void)mail.answer(loom::Ack{});
    }
    /// A VALUE OR A REFERENCE PLACED ON A VIEW'S AREA. A value's place reads back to the row and
    /// column of the rows drawn there it fell on, which only a field copy spends, and only on a
    /// value view; a whole value and a reference open their subject wherever they land.
    void on(const ws::PaneCanvasValueDrop& drop, loom::Mail& mail) {
        dropped(drop.pane, drop.area, drop.x, drop.y, drop.data, false,
                DropWord{drop.under, drop.part, drop.drawing}, mail);
    }
    void on(const ws::PaneCanvasDrop& drop, loom::Mail& mail) {
        dropped(drop.pane, drop.area, drop.x, drop.y, drop.data, true,
                DropWord{drop.under, drop.part, drop.drawing}, mail);
    }
    /// What the window drew under the hand where a drop was let go (`ws::PaneCanvasValueDrop`).
    struct DropWord {
        std::string under, part;
        std::int64_t drawing = 0;
    };
    /// A field copy placed on a row the window drew another element on than the view shows there
    /// now fills nothing (`WindowDrawing::moved`).
    void dropped(const std::string& key, const std::string& area, std::int64_t x, std::int64_t y,
                 const loom::Bytes& data, bool reference, const DropWord& drawn, loom::Mail& mail) {
        pane::ValueView* v = view_of(key);
        if (!mail.authored_from_role(kWorkshopRole) || !v || canvas_of(*v).area() != area) return;
        const bool moved = canvas_of(*v).moved(drawn.under, drawn.part, drawn.drawing, x, y);
        if (v->is_default() && (properties_.drafting() || properties_.committing())) {
            properties_.tell("Finish the pane-property edit before inspecting an inventory entry");
        } else {
            pane::ViewContext c{mail, asked_};
            if (reference) {
                v->drop_reference(data, c);
            } else {
                const ws::RowCell at = canvas_of(*v).cell_at(x, y);
                v->drop_value(data, at.row, at.column, value_mode(*v) && !moved, c);
            }
            if (v->is_default() && !v->active) properties_.tell(v->notice());
        }
        declare(*v, mail); say(*v, mail);
    }
    void on(const ws::PaneCarryAnswered& answer, loom::Mail& mail) {
        for (auto& v : views_) if (v.hear(answer, mail)) { declare(v, mail); say(v, mail); return; }
    }
    void on(const PaneOperationAnswered& answer, loom::Mail& mail) { route(answer, mail); }
    void on(const zengine::inventory::InventoryEntry& answer, loom::Mail& mail) { route(answer, mail); }
    void on(const loom::Refused& answer, loom::Mail& mail) { route(answer, mail); }
    void on(const ws::PaneObservationAnswered& answer, loom::Mail& mail) { route(answer, mail); }
    void on(const loom::PokeStructure& answer, loom::Mail& mail) { route(answer, mail); }

    /// AN INVALIDATION FROM THE INVENTORY OFFICE, never a value: watching views run one cycle.
    void on(const zengine::inventory::InventoryChanged&, loom::Mail& mail) {
        if (!mail.authored_from_role(zengine::inventory::kInventoryRole)) return;
        pane::ViewContext c{mail, asked_};
        for (auto& v : views_) {
            const bool was = v.watching();
            v.changed(c);
            if (was && !v.watching()) say(v, mail);
        }
    }

    void on(const ws::PaneLaunchAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask()) return;
        for (auto& launch : launches_) {
            if (launch.correlation == 0 || launch.correlation != mail.correlation()) continue;
            launch.correlation = 0;
            pane::ValueView& v = views_[launch.slot - 1];
            if (answer.refusal.empty() || v.incarnation() != launch.incarnation) return;
            if (!v.has_entry()) {
                pane::ViewContext c{mail, asked_};
                if (v.retire(c, {})) offer(v, mail);
                views_[launch.from - 1].say("New view not shown: " + answer.refusal);
                say(views_[launch.from - 1], mail);
            } else {
                v.say("This view could not be shown: " + answer.refusal + " -- open it from the Pane Manager");
            }
            return;
        }
    }
    void on(const ws::PaneCloseAnswered&, loom::Mail&) {}

    /// THE PANES WHOSE MANUAL IS THIS WEAVE'S, any of its four views: the whole page is the
    /// weave's, from the page this image was built with, so a reloaded image answers its own words.
    bool known_pane(const std::string& key) { return view_of(key) != nullptr; }

    // ---- Its views in graphics windows (its office's `MultiPaneFacet` calls these) ------------

    /// EACH VIEW'S SEAT, where a window that shows the view's area finds it.
    template <class F>
    void each_seat(F&& visit) {
        for (ws::PaneCanvasSeat& seat : canvases_) visit(seat);
    }

    /// A view composed again for the room a window gave it.
    void on_window_room(const std::string& key, loom::Mail& mail) {
        if (pane::ValueView* v = view_of(key)) {
            fit_room(*v);
            say(*v, mail);
        }
    }

    /// THE POINTER ON A VIEW IN A WINDOW, read onto the rows drawn there: a press arms a value
    /// view's field pickup under the window's number for it, its motion spends it, the wheel walks
    /// the fields and a right press opens the view's menu.
    void on_window_pointer(const std::string& key, const ws::WindowPointer& p, bool,
                           loom::Mail& mail) {
        pane::ValueView* v = view_of(key);
        if (!v) return;
        const auto number = static_cast<std::int64_t>(p.press);
        if (p.phase == ws::canvas_pointer::kWheel) {
            if (value_mode(*v) && v->wheel(p.dy)) say(*v, mail);
        } else if (p.phase == ws::canvas_pointer::kMove) {
            if (value_mode(*v) && number != 0 && number == held_of(*v)) {
                pane::ViewContext c{mail, asked_};
                v->dragged(p.at.row, p.at.column, c);
            }
        } else if (p.phase == ws::canvas_pointer::kPress && p.button == 3) {
            right_press(*v, p.at, /*current=*/!p.moved, mail);
        } else if (p.phase == ws::canvas_pointer::kPress && p.button == 1) {
            held_of(*v) = p.at.shown && value_mode(*v) && !p.moved ? number : 0;
            if (p.at.shown || p.moved) press(*v, p.at, /*current=*/!p.moved, mail);
        } else if (p.button == 1) {
            held_of(*v) = 0; // released, or a hold the window ended
        }
    }

    /// A PRESS ASKED AS A PART'S ACTION, by a window that shows a view in an area placed deaf to the
    /// pointer: a primary press on its place in the rows drawn there. No press the window holds
    /// stands behind it, so it arms no field pickup.
    void on_window_press(const std::string& key, std::int64_t row, std::int64_t column,
                         loom::Mail& mail) {
        pane::ValueView* v = view_of(key);
        if (!v) return;
        held_of(*v) = 0;
        press(*v, ws::RowCell{row, column, true}, /*current=*/true, mail);
    }

    /// A RIGHT PRESS on the view's rows opens its own menu; one beside them, where a prose press
    /// would have named no row, is handed back, so Workshop's own pane menu opens there.
    void right_press(pane::ValueView& v, const ws::RowCell& at, bool current, loom::Mail& mail) {
        if (!at.shown) {
            (void)ws::pane_menu::pass_back(mail, pane::kInfoPaneRole, v.key());
            return;
        }
        if (!value_mode(v) && !current) {
            properties_.tell("This view changed -- press again");
            say(v, mail);
            return;
        }
        if (!value_mode(v)) {
            // THE PANE-PROPERTY VIEW'S OWN MENU: the two ways out of it.
            auto offer_menu = ws::pane_menu::Offer(v.key(), "properties").at(at.row, at.column)
                                  .row(pane::kActionViewNew, "New value view");
            if (v.has_entry()) offer_menu.row(pane::kActionInventory, "Show the value view");
            menus_[v.slot() - 1] = offer_menu.send(mail, pane::kInfoPaneRole);
            return;
        }
        if (!current) { v.say("This view changed -- press again"); say(v, mail); return; }
        open_menu(v, at.row, at.column, mail);
    }
    void on(const ws::PaneMenuAnswered& answer, loom::Mail& mail) {
        for (auto& v : views_) {
            const std::string id = menus_[v.slot() - 1].take(mail, answer);
            if (id.empty()) continue;
            if (!value_mode(v)) {
                if (id == pane::kActionInventory && v.has_entry()) { v.active = true; declare(v, mail); say(v, mail); }
                else if (id == pane::kActionViewNew) new_view(v, false, mail);
                return;
            }
            act_view(v, id, mail);
            return;
        }
    }
    /// AN ACCEPTED ACTIVATION offers the views (`announce`) and ends each lease no view here holds.
    void on_activated(loom::Mail& mail) {
        announce(mail);
        // AN ARRIVING IMAGE HOLDS NO OBSERVATION. A reload keeps this office's WeaveId, so
        // Workshop cannot tell a predecessor's leases from ours; lease 0 ends every one this
        // holder has on the pane. Only on activation: a catalog request re-announces live views.
        for (const auto& v : views_)
            if (!v.watching())
                (void)mail.as_role(pane::kInfoPaneRole)
                    .send_to_role(kWorkshopRole, ws::PaneObservationEnded{v.key(), 0});
    }

    /// THE ONE INVENTORY, AS THE HOST SAYS IT -- published when it changes, or answered to this
    /// image's arrival -- read for which of this office's views are on the desk: a hidden view
    /// keeps its draft, not a watch.
    void on(const PaneInventory& said, loom::Mail& mail) {
        if (!from_workshop(mail)) {
            return; // a stranger's list of panes is not the host's inventory
        }
        pay_declarations(mail);
        pane::ViewContext c{mail, asked_};
        for (auto& v : views_) {
            bool open = false;
            for (const auto& p : said.panes) if (p.office == pane::kInfoPaneRole && p.pane == v.key()) open = p.open;
            const bool was = v.watching();
            v.desk(open, c);
            if (was && !v.watching()) say(v, mail);
        }
    }

    /// WHAT THE SUBJECT LOOKS LIKE, SAID BY THE HOST, AND WHAT ITS ROWS ADDRESS: the property
    /// view's reading (`PropertyView::shown`).
    void on(const PaneSubjectShown& said, loom::Mail& mail) {
        if (!from_workshop(mail)) {
            return; // a stranger's opinion about a pane is not a reading of one
        }
        properties_.shown(said);
        settle(mail);
    }

    /// THE SUBJECT'S SETTINGS, AS THE LAYOUTS' OWNER READS THEM (`PropertyView::settings_shown`),
    /// and what it made of a commit of one (`PropertyView::acted_setting`): its answers alone.
    void on(const ws::PaneSettingsAnswered& read, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return; // an answer is Loom's to verify, to an ask this pane sent
        }
        properties_.settings_shown(read, mail.correlation());
        settle(mail);
    }
    void on(const ws::PaneSettingCommitAnswered& said, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        properties_.acted_setting(said, mail);
        settle(mail);
    }

    /// What the owner made of an ask the property view is waiting on (`PropertyView::acted`).
    void on(const PaneSubjectActed& said, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        properties_.acted(said, mail);
        settle(mail);
    }

    /// AN OFFICE ASKS THIS PANE TO INSPECT A PANE -- the Pane Manager's `inspect in Info` row --
    /// and the ask is the property view's own inspect (`PropertyView::inspect_asked`).
    void on(const InspectPaneRequested& asked, loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            return; // an office asks; personal speech is answered by nobody
        }
        pane::ViewContext c{mail, asked_};
        properties_.inspect_asked(asked, c);
        settle(mail);
    }

    /// Loom's word that an ask of this pane's never reached the door's handler
    /// (`zen.DispatchRefused`; WL-INFO-13): an attestation, not an answer. Each value view is
    /// offered it first; then, with Loom's provenance, the property view's two records.
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        // A DECLARATION LOOM REFUSED -- its host gone as it crossed -- IS OWED, and said again
        // beside the next word the host sends (`settle`, `on(PaneInventory)`).
        if (mail.dispatch_refused() && refused.shape == PaneActions::zen_name) {
            declarations_owed_ = true;
            return;
        }
        pane::ViewContext c{mail, asked_};
        for (auto& v : views_) if (v.hear(refused, c)) { declare(v, mail); say(v, mail); return; }
        if (!mail.dispatch_refused()) {
            return;
        }
        if (properties_.refused(refused, mail)) {
            settle(mail);
        }
    }

    /// A PRESS NAMES A ROW OF A VIEW'S ROOM, and the view knows what that row is because it
    /// composed it: `current` says the press was aimed at what the view shows now, so a stale or
    /// clipped place is refused.
    void press(pane::ValueView& v, const ws::RowCell& at, bool current, loom::Mail& mail) {
        if (!value_mode(v)) {
            if (!current) {
                properties_.tell("This view changed -- press again");
                say(v, mail);
                return;
            }
            properties_.press(at.row);
            settle(mail);
            return;
        }
        const std::string control = v.press(at.row, at.column, current, mail.correlation());
        if (control.empty()) { say(v, mail); return; }
        if (control == pane::kActionViewMenu) { open_menu(v, at.row, at.column, mail); return; }
        act_view(v, control, mail);
    }

    /// ONLY A DRAFT READS RAW KEYS. The property view's line consumes them through this pane's
    /// clipboard, and a copy or a paste it asked for is this pane's to carry.
    void on(const PaneKey& key, loom::Mail& mail) {
        pane::ValueView* v = view_of(key.pane);
        if (!mail.authored_from_role(kWorkshopRole) || !v) {
            return;
        }
        // Escape's default: a view holds nothing selected (its drafts answer Escape with their own
        // rows), so Escape goes back to Workshop.
        if (ws::pane_escape::answer(key, mail, pane::kInfoPaneRole, [] { return false; })) {
            return;
        }
        if (value_mode(*v)) { v->key(key); say(*v, mail); return; }
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (!properties_.key(key.scancode, key.modifiers, clip_)) {
            return; // a key that means nothing to the line is no act: the notice stands, unsaid
        }
        if (clip_.writes != copied_before) {
            mail.publish(gfx::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_paste(mail);
        }
        settle(mail);
    }

    void on(const PaneTextInput& typed, loom::Mail& mail) {
        pane::ValueView* v = view_of(typed.pane);
        if (!mail.authored_from_role(kWorkshopRole) || !v) {
            return;
        }
        if (value_mode(*v)) { v->text(typed.text); say(*v, mail); return; }
        properties_.type(typed.text);
        settle(mail);
    }

    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        pane::ValueView* v = view_of(asked.pane);
        if (!mail.authored_from_role(kWorkshopRole) || !v) {
            return;
        }
        if (value_mode(*v)) {
            if (!v->answers(asked.id)) return; // a raced or undeclared id is no act
            if (asked.id == pane::kActionViewMenu) { open_menu(*v, 0, 0, mail); return; }
            act_view(*v, asked.id, mail);
            return;
        }
        if (asked.id == pane::kActionInventory && !properties_.drafting() && v->has_entry()) {
            v->active = true; declare(*v, mail); say(*v, mail); return;
        }
        if (asked.id == pane::kActionViewNew && !properties_.drafting()) {
            new_view(*v, false, mail); return;
        }
        // THE MODE OWNS THE PANE'S ACTIONS FIRST, AND AN ID IT DOES NOT ANSWER TO IS NO ACT.
        // While a draft is open the view declares two rows and no more -- but the declaration
        // and the keystroke race across two messages (Escape and Return in one poll resolve to a
        // cancel and a commit, and the commit arrives after the cancel closed the draft), so a
        // stale id, or one nobody declared, must mean nothing: no commit, and no notice spent.
        if (!properties_.answers(asked.id)) {
            return;
        }
        pane::ViewContext c{mail, asked_};
        properties_.act(asked.id, c);
        settle(mail);
    }

    void on(const gfx::ClipboardCopy& said, loom::Mail&) {
        // WHAT THE PROCESS SAYS IT COPIED -- this pane's own copies included, which is why
        // it is a mirror rather than a second store: a paste answers with this.
        clip_.text = said.text;
    }

    void on(const gfx::ClipboardText& a, loom::Mail& mail) {
        if (!mail.answers_ask() || !paste_.awaiting || mail.correlation() != paste_.pending) {
            return;
        }
        paste_.awaiting = false;
        properties_.pasted(a.readable ? a.text : clip_.text, paste_.epoch);
        settle(mail);
    }

private:
    /// THE HOST, SPEAKING AS ITS OFFICE OR ANSWERING THIS IMAGE'S OWN ASK. A host answer carries
    /// answer provenance, not office authorship (Loom ANS-01), and it can only come from the
    /// office this image asked.
    static bool from_workshop(const loom::Mail& mail) {
        return mail.authored_from_role(kWorkshopRole) || mail.answers_ask();
    }

public:
    // ---- Offering and declaring ---------------------------------------------------------

    /// THE DEFAULT PANE, AND EVERY SLOT THIS IMAGE HAS OFFERED BEFORE. A slot is offered on first
    /// use and reused after, so the catalog never holds more than four Info panes and a session
    /// that never makes a view sees exactly the one pane it always did.
    void announce(loom::Mail& mail) {
        for (auto& v : views_) {
            if (!v.is_default() && !v.offered) continue;
            offer(v, mail);
            declare(v, mail);
        }
        // ...AND WHAT THE HOST HOLDS NOW: the inventory, and the subject this office named --
        // which a reload of this image finds standing, because the host keeps it. Both are
        // published when they change; an image arriving while nothing changes would otherwise
        // wait for an unrelated change.
        (void)mail.as_role(pane::kInfoPaneRole)
            .send_to_role(kWorkshopRole, PaneInventoryRequested{});
        (void)mail.as_role(pane::kInfoPaneRole)
            .send_to_role(kWorkshopRole, PaneSubjectRequested{});
    }

private:
    /// WHAT THIS PANE ANSWERS TO RIGHT NOW -- re-declared whenever the mode changes.
    void declare(pane::ValueView& v, loom::Mail& mail) {
        PaneActions actions;
        actions.pane = v.key();
        actions.rows = value_mode(v) ? v.actions() : property_rows();
        (void)mail.as_role(pane::kInfoPaneRole).send_to_role(kWorkshopRole, actions);
    }
    /// ...EVERY VIEW'S, AGAIN, while a refused one is owed.
    void pay_declarations(loom::Mail& mail) {
        if (!declarations_owed_) return;
        declarations_owed_ = false;
        for (auto& v : views_) declare(v, mail);
    }

    /// THE DEFAULT PANE'S ROWS WHILE IT SHOWS PANE PROPERTIES: the property view's own, then,
    /// while no draft is open, the two ways to a value view.
    std::vector<PaneActionRow> property_rows() const {
        std::vector<PaneActionRow> rows = properties_.action_rows();
        if (properties_.drafting()) return rows;
        if (views_.front().has_entry()) rows.push_back({pane::kActionInventory, "inventory entry",
            input::scan::kI, input::mod::kCtrl, {}, {}, {}, input::scan::kRight, input::mod::kAlt});
        rows.push_back({pane::kActionViewNew, "new value view", input::scan::kN, input::mod::kCtrl,
            {}, {ws::cause_of<ws::PaneLaunchRequested>(), ws::cause_of<ws::PaneSettingsRequested>()}});
        return rows;
    }

    /// WHAT THE PROPERTY VIEW SAYS IT IS OWED after an act or an answer: the default pane's rows
    /// declared again, then its picture said again.
    void settle(loom::Mail& mail) {
        pay_declarations(mail);
        const pane::PropertyView::Due due = properties_.take_due();
        if (due.settings) {
            pane::ViewContext c{mail, asked_};
            properties_.ask_settings(c);
        }
        if (due.declare) declare(views_.front(), mail);
        if (due.say) say(views_.front(), mail);
    }

    void offer(pane::ValueView& v, loom::Mail& mail) {
        v.offered = true;
        (void)mail.as_role(pane::kInfoPaneRole).send_to_role(kWorkshopRole, v.is_default()
            ? PaneOffered{pane::kInfoPane, pane::kInfoPaneName, pane::kInfoPaneSummary, 13, 58}
            : PaneOffered{v.key(), v.title(), pane::kInfoViewSummary, 16, 60});
    }

    // ---- Independent views --------------------------------------------------------------

    pane::ValueView* view_of(const std::string& key) {
        for (auto& v : views_) if (v.key() == key) return &v;
        return nullptr;
    }
    /// The slots show typed values always; the default view only while its value is shown.
    static bool value_mode(const pane::ValueView& v) { return !v.is_default() || v.active; }

    /// ONE VIEW'S AREA (`ws::PaneCanvasSeat`): the window that shows it and the rows drawn there.
    /// Kept here, per slot, so a view's retirement keeps the room.
    ws::PaneCanvasSeat& canvas_of(const pane::ValueView& v) { return canvases_[v.slot() - 1]; }
    /// The gesture of the last primary press on a view, while it was on a value view's rows.
    std::int64_t& held_of(const pane::ValueView& v) { return held_[v.slot() - 1]; }

    /// The rows and columns a view composes for, as its seat measured them -- and, for the
    /// default view, the property view's too.
    void fit_room(pane::ValueView& v) {
        const ws::PaneCanvasSeat& canvas = canvas_of(v);
        v.room(canvas.rows(), canvas.columns());
        if (v.is_default()) properties_.room(canvas.rows(), canvas.columns());
    }

    /// THE ROWS DRAWN in the view's area, the caret and selection standing in them.
    void send(pane::ValueView& v, ws::ComposedRows said, const ws::RowsCaret& caret,
              loom::Mail& mail) {
        canvas_of(v).publish_rows(mail, std::move(said.rows), std::move(said.parts), caret,
                                  said.number);
    }
    bool can_create() const {
        return std::any_of(views_.begin() + 1, views_.end(), [](const pane::ValueView& v) { return !v.allocated(); });
    }

    /// EVERY ANSWER IS OFFERED TO EACH VIEW IN TURN; only the view whose own record it answers
    /// takes it (Loom answer provenance, then that record's unique number).
    template <class Answer>
    void route(const Answer& answer, loom::Mail& mail) {
        pane::ViewContext c{mail, asked_};
        for (auto& v : views_) {
            if (v.hear(answer, c)) {
                declare(v, mail);
                say(v, mail);
                return;
            }
        }
    }

    /// ONE ACT, WHETHER A CONTROL WAS PRESSED, ITS KEY WAS TYPED OR A MENU ROW CHOSEN.
    void act_view(pane::ValueView& v, const std::string& id, loom::Mail& mail) {
        v.say({});
        if (id == pane::kActionViewNew) new_view(v, false, mail);
        else if (id == pane::kActionViewFork) new_view(v, true, mail);
        else if (id == pane::kActionViewClose) close_view(v, mail);
        else if (id == pane::kActionViewUse) {
            if (!v.allocated()) v.allocate();
            v.say("This view is yours: drag an Inventory entry here");
        } else {
            pane::ViewContext c{mail, asked_};
            const pane::ActResult result = v.act(id, c);
            if (result.reoffer && !v.is_default()) offer(v, mail);
            if (result.leave) v.active = false;
        }
        declare(v, mail);
        say(v, mail);
    }

    /// NEW OR FORK: the lowest unused slot, launched onto the desk. A fork takes the local draft
    /// only -- never a pending request, a watch or its lease.
    void new_view(pane::ValueView& from, bool fork, loom::Mail& mail) {
        pane::ValueView* slot = nullptr;
        for (auto it = views_.begin() + 1; it != views_.end() && !slot; ++it) if (!it->allocated()) slot = &*it;
        if (!slot) {
            std::string hidden;
            for (auto it = views_.begin() + 1; it != views_.end(); ++it)
                if (!it->on_desk()) hidden += (hidden.empty() ? "" : ", ") + it->title();
            tell(from, "All four Info views are in use -- close one first" +
                       (hidden.empty() ? std::string() : " (hidden: " + hidden + "; the Pane Manager reopens them)"), mail);
            return;
        }
        if (fork && !from.has_entry()) { tell(from, "Fork view unavailable: this view holds no value", mail); return; }
        slot->allocate();
        if (fork) slot->fork_from(from);
        else slot->say("New view: drag an Inventory entry here, or a field onto a field");
        offer(*slot, mail);
        const auto correlation = ++asked_;
        launches_.push_back({correlation, slot->slot(), from.slot(), slot->incarnation()});
        if (launches_.size() > pane::kMaxInfoViews) launches_.erase(launches_.begin());
        (void)mail.as_role(pane::kInfoPaneRole)
            .send_to_role(kWorkshopRole, ws::PaneLaunchRequested{pane::kInfoPaneRole, slot->key()}, correlation);
        declare(*slot, mail);
        say(*slot, mail);
        tell(from, (fork ? "Forked into " : "Opened ") + slot->title(), mail);
    }

    /// A SENTENCE FOR THE VIEW THAT ACTED -- in the pane-property notice while the default pane
    /// shows pane properties, where a value view's notice would be read by nobody.
    void tell(pane::ValueView& v, std::string sentence, loom::Mail& mail) {
        if (value_mode(v)) v.say(std::move(sentence));
        else properties_.tell(std::move(sentence));
        say(v, mail);
    }

    /// CLOSE RETIRES THE SLOT: a pending save refuses; unsaved edits or other pending work need a
    /// second Close. Inventory data is never touched; the slot's offer stays for the next view.
    void close_view(pane::ValueView& v, loom::Mail& mail) {
        if (v.is_default()) { v.say("Close view unavailable: the default Info view stays; Discard clears its edits"); return; }
        if (v.saving()) { v.say("Close waits: a save is waiting for its answer"); return; }
        if ((v.dirty() || v.busy() || v.editing()) && !v.close_armed()) {
            v.arm_close();
            v.say(std::string(v.dirty() ? "Unsaved edits" : "Pending work") +
                  " in this view: press Close again to discard and close (Inventory data stays)");
            return;
        }
        pane::ViewContext c{mail, asked_};
        if (v.retire(c, {})) offer(v, mail);
        (void)mail.as_role(pane::kInfoPaneRole)
            .send_to_role(ws::kLayoutsRole, ws::PaneCloseRequested{pane::kInfoPaneRole, v.key()}, ++asked_);
    }

    void open_menu(pane::ValueView& v, std::int64_t row, std::int64_t column, loom::Mail& mail) {
        auto menu = ws::pane_menu::Offer(v.key(), "view").at(row, column);
        for (const auto& [id, label] : v.menu_rows()) menu.row(id, label);
        menus_[v.slot() - 1] = menu.send(mail, pane::kInfoPaneRole);
        if (!menus_[v.slot() - 1].pending()) { v.say("The actions menu could not be requested"); say(v, mail); }
    }

    // ---- The clipboard ------------------------------------------------------------------

    /// A PASTE THE PROPERTY VIEW'S LINE ASKED FOR, read against the draft that asked: the
    /// correlation says the answer is to this pane's ask (`on(ClipboardText)`).
    void begin_paste(loom::Mail& mail) {
        paste_.pending = ++asked_;
        paste_.epoch = properties_.draft_epoch();
        paste_.awaiting = true;
        (void)mail.as_role(pane::kInfoPaneRole)
            .send_to_role(canvases_.front().clipboard_office(), gfx::ClipboardTextRequested{},
                          paste_.pending);
    }

    // ---- Saying what the pane shows -------------------------------------------------------

    /// ONE VIEW'S PICTURE: the pane-property view's, or a value view's own.
    void say(pane::ValueView& v, loom::Mail& mail) {
        if (!value_mode(v)) { say_properties(mail); return; }
        if (!v.granted()) return;
        send(v, v.draw(can_create()), v.caret(), mail);
    }

    /// THE PANE-PROPERTY VIEW'S ROWS, sent with the names it gives them
    /// (`PropertyView::named_rows`). They number no meaning of their own: a value view's picture
    /// is never theirs.
    void say_properties(loom::Mail& mail) {
        if (!properties_.granted()) {
            return;
        }
        std::vector<ws::TextRow> rows = properties_.compose();
        const auto said = static_cast<std::int64_t>(rows.size());
        send(views_.front(), ws::ComposedRows{std::move(rows), properties_.named_rows(said)},
             properties_.caret(), mail);
    }

    // ---- State not in the shape ------------------------------------------------------------

    struct Paste {
        bool awaiting = false;
        std::uint64_t pending = 0;
        /// THE DRAFT THAT ASKED (`component::TextBox::draft_epoch`): the correlation says the
        /// answer is to this pane's ask, and the epoch says the line it was asked for still
        /// stands.
        std::uint64_t epoch = 0;
    };

    /// THE FOUR VIEWS, in slot order: `info` first. Each owns its own draft and records.
    std::vector<pane::ValueView> views_;
    /// THE DEFAULT PANE'S PANE-PROPERTY VIEW, shown while `info` holds no value.
    pane::PropertyView properties_;
    /// Each view's one outstanding menu -- image-local, so a reload cancels them.
    std::array<ws::pane_menu::Asked, pane::kMaxInfoViews> menus_{};
    /// Each view's rooms -- image-local: a reload waits for a fresh room -- and its held press.
    std::array<ws::PaneCanvasSeat, pane::kMaxInfoViews> canvases_{};
    std::array<std::int64_t, pane::kMaxInfoViews> held_{};
    /// Launches this image asked for, so a refused one can report to the view that asked.
    struct Launch {
        std::uint64_t correlation = 0;
        std::size_t slot = 0, from = 0;
        std::uint64_t incarnation = 0;
    };
    std::vector<Launch> launches_;

    Paste paste_;
    component::Clipboard clip_;

    /// ONE COUNTER FOR EVERY QUESTION THIS PANE ASKS, so a correlation is this incarnation's
    /// own and an answer to somebody else's question is not mistaken for one to ours.
    std::uint64_t asked_ = 0;
    bool declarations_owed_ = false; ///< a declaration Loom refused, said again with the next word
};

} // namespace

ZEN_EXPORT_WEAVE(InfoPaneWeave)
