// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
// The View Builder: a pane beside Flow where a view is made by hand on a design canvas the view
// draws itself on, and run through the view host. Law: agents/view.md. Guide:
// docs/workshop/view-builder.md.
#include "view-builder/picture.hpp"
#include "view-builder/vocabulary.hpp"
#include "view/vocabulary.hpp"

#include "activation/activation.hpp"
#include "input/vocabulary.hpp"
#include "inventory/codec.hpp"
#include "message-draft/transfer.hpp"
#include "workshop/actor_scope.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/kernel/schema_codec.hpp>
#include <zen/registry.hpp>
#include <zen/weave.hpp>
#include <zen/weave/dispatch_refusal.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace {
namespace vb = zengine::view_builder;
namespace view = zengine::view;
namespace ws = zengine::workshop;
namespace input = zengine::input;
namespace surface = zengine::surface;
constexpr const char* workshop_role = "zengine.workshop";

/// The shape a carried value names, and the field when the carry was one field: a description's
/// shape, decoded with the shapes it carries; a field's root shape and its top-level name; or a
/// value's own shape.
std::pair<std::shared_ptr<const loom::Schema>, std::optional<std::string>> label_source(const loom::Value& value) {
    if (zengine::flow::shape::is_description(value)) return {zengine::flow::shape::described(value), std::nullopt};
    if (zengine::message_draft::is_field_value(value)) {
        const auto& bytes = value.get("library")->as_bytes();
        const auto library = zengine::message_draft::read_library(
            std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        const auto& path = value.get("path")->as_list();
        if (library.entries().size() != 1 || path.size() != 1 || !path[0].as_message()->get("field"))
            throw std::invalid_argument("a label shows a top-level field; carry that field, or its whole value");
        return {library.entries().front().draft.schema(), path[0].as_message()->get("field")->as_text()};
    }
    return {value.schema_ptr(), std::nullopt};
}

class ViewBuilderPane final
    : public loom::WeaveBase<
          ViewBuilderPane, vb::ViewBuilderState,
          loom::Accept<loom::Activated, ws::PaneCatalogRequested, ws::PaneRoom, ws::PaneCanvasRoom,
                       ws::PaneCanvasPointer, ws::PaneCanvasHover, ws::PaneCanvasRejected, ws::PaneKey,
                       ws::PaneTextInput, ws::PaneActionRequested, ws::ActionsJudged, ws::PaneQuitRequested,
                       ws::PaneCanvasValueDrop, ws::PaneMenuAnswered, ws::PaneOperationAnswered,
                       ws::PaneCarryAnswered, vb::ViewEdit, view::ViewAnswer, loom::DispatchRefused,
                       ws::ProjectRoot>,
          loom::Emit<ws::v3::PaneOffered, ws::PaneContent, ws::v4::PaneCanvasContent, ws::PaneActions,
                     ws::PaneEscapeUnspent, ws::PanePassRequested, ws::PaneMenuRequested,
                     ws::PaneQuitAnswered, ws::PaneOperationRequested, ws::v2::PaneOperationRequested,
                     ws::PaneValueCarryRequested, vb::ViewEdited, view::ViewRun, view::ViewResume,
                     view::ViewApply, view::ViewStop, ws::ProjectRootRequested>> {
public:
    loom::Value snapshot() const override {
        vb::ViewBuilderState saved;
        try {
            const auto bytes = view::description_bytes(model_.description);
            saved.description.assign(bytes.begin(), bytes.end());
        } catch (const std::exception&) {
        }
        saved.path = model_.path;
        saved.file = file_;
        saved.dirty = model_.dirty;
        saved.running = model_.running;
        saved.kept = kept_;
        return loom::to_value(saved);
    }

    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) return;
        // A SAVE STILL WAITING ON WORKSHOP ends with this activation, and is said: its answer finds
        // nothing waiting, and a successor never holds one.
        if (std::exchange(saving_, std::nullopt))
            model_.notice = "The save waiting for Workshop was dropped; save again";
        if (!restored_) {
            restored_ = true;
            // A FIRST IMAGE IS A LAUNCH, and runs again what the builder ran: its project file says
            // which, once the project answers. A successor keeps what its predecessor held.
            launch_ = state_.description.empty();
            if (!state_.description.empty()) {
                auto read = view::read_description(std::string_view(
                    reinterpret_cast<const char*>(state_.description.data()), state_.description.size()));
                if (read) {
                    model_.description = std::move(read.description);
                    model_.path = state_.path;
                    model_.dirty = state_.dirty;
                    model_.running = state_.running;
                    file_ = state_.file;
                    kept_ = state_.kept;
                } else {
                    model_.notice = "Could not restore the View Builder: " + read.reason;
                }
            }
            root_ask_ = ++correlation_;
            (void)mail.as_role(vb::kRole).send_to_role(ws::kProjectRole, ws::ProjectRootRequested{}, root_ask_);
        }
        offer(mail);
    }
    /// WHERE THE PROJECT IS: where this builder's own file lives, what that file holds, read
    /// rather than assumed, and, at a launch, what it runs. A successor writes there at once what
    /// its predecessor could not.
    void on(const ws::ProjectRoot& said, loom::Mail& mail) {
        if (!mail.answers_ask() || mail.correlation() != root_ask_) return;
        project_dir_ = said.project_dir;
        const bool launch = std::exchange(launch_, false);
        if (project_dir_.empty()) {
            if (!launch) return;
            model_.notice = "No project directory: the view this builder runs is not remembered across a launch";
            show(mail);
            return;
        }
        std::string why;
        remembered_ = recorded(why);
        if (launch) {
            if (!remembered_) model_.notice = why;
            resume(mail);
        } else {
            remember();
        }
        show(mail);
    }
    void on(const ws::PaneCatalogRequested&, loom::Mail& mail) {
        if (mail.authored_from_role(workshop_role)) offer(mail);
    }
    void on(const ws::PaneRoom& room, loom::Mail& mail) {
        if (!host(mail, room.pane)) return;
        rows_ = room.rows;
        columns_ = room.columns;
        show(mail);
    }
    void on(const ws::PaneCanvasRoom& room, loom::Mail& mail) {
        if (!host(mail, room.pane)) return;
        // A fresh room ends whatever the old one held: a drag, a hover, a mark. The pan stays,
        // held within what the new room reaches.
        room_ = room;
        pictures_.clear();
        held_.reset();
        shown_.hovered.reset();
        shown_.landing.reset();
        shown_.ghost.reset();
        shown_.met_x.reset();
        shown_.met_y.reset();
        show(mail);
    }
    void on(const ws::PaneCanvasRejected& answer, loom::Mail& mail) {
        if (host(mail, answer.pane) && answer.grant == room_.grant)
            model_.notice = "Drawing rejected: " + answer.reason;
    }
    void on(const ws::ActionsJudged& answer, loom::Mail& mail) {
        if (host(mail, answer.pane) && !answer.accepted) {
            model_.notice = "View Builder actions: " + answer.refusal;
            show(mail);
        }
    }
    void on(const ws::PaneActionRequested& action, loom::Mail& mail) {
        if (!host(mail, action.pane)) return;
        if (shown_.box && !commit()) {
            show(mail);
            return;
        }
        if (action.id == "save") ask_save(mail);
        else perform(action.id, {}, mail);
        show(mail);
    }
    void on(const ws::PaneQuitRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(workshop_role)) return;
        const std::string why = !pending_.empty() ? "The View Builder is waiting for the view host. Quit again after it answers."
                                : model_.dirty    ? "The View Builder has an unsaved view. Save it before quitting."
                                                  : "";
        (void)mail.answer(ws::PaneQuitAnswered{vb::kPane, why.empty(), why});
    }
    void on(const ws::PaneKey& key, loom::Mail& mail) {
        if (!host(mail, key.pane)) return;
        if (shown_.box) {
            type_key(key);
            show(mail);
            return;
        }
        if (key.scancode == input::scan::kEscape) {
            if (model_.choosing) model_.choosing.reset();
            else if (model_.selected) model_.selected.reset();
            else {
                (void)mail.as_role(vb::kRole).send_to_role(workshop_role, ws::PaneEscapeUnspent{vb::kPane}, mail.correlation());
                return;
            }
        } else if (key.scancode == input::scan::kDelete && model_.selected) {
            perform("remove", {std::to_string(*model_.selected)}, mail);
        } else if (model_.selected && nudge(key)) {
        } else {
            return;
        }
        show(mail);
    }
    void on(const ws::PaneTextInput& text, loom::Mail& mail) {
        if (!host(mail, text.pane) || !shown_.box) return;
        auto& box = shown_.box->text;
        if (box.size() + text.text.size() > kMaxTyped) model_.notice = "A value holds at most " + std::to_string(kMaxTyped) + " bytes";
        else box.type(text.text);
        show(mail);
    }
    void on(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
        if (!host(mail, event.pane) || event.grant != room_.grant) return;
        const bool ours = held_ && held_->gesture == event.gesture;
        if (event.phase == ws::canvas_pointer::kMove) {
            if (ours) drag(event, mail);
            return;
        }
        if (event.phase == ws::canvas_pointer::kRelease || event.phase == ws::canvas_pointer::kLost) {
            if (ours) finish(event, mail);
            return;
        }
        if (event.phase == ws::canvas_pointer::kWheel) {
            scroll(event, mail);
            return;
        }
        if (event.phase == ws::canvas_pointer::kPress) press(event, mail);
    }
    /// WHERE THE POINTER RESTS: the element under it is marked, and while a value is carried, the
    /// label it would land on. Nothing else moves.
    void on(const ws::PaneCanvasHover& hover, loom::Mail& mail) {
        if (!host(mail, hover.pane) || hover.grant != room_.grant) return;
        std::optional<std::size_t> over, landing;
        if (hover.over && !pictures_.empty()) {
            const auto pictured = std::find_if(pictures_.begin(), pictures_.end(),
                                               [&](const auto& p) { return p.content.picture == hover.picture; });
            const auto& picture = pictured == pictures_.end() ? pictures_.back() : *pictured;
            if (const auto* hit = picture.hit(hover.x, hover.y)) over = element_of(*hit);
            if (over && hover.carrying) {
                if (model_.description.elements.at(*over).kind == view::Kind::label) landing = over;
                over.reset();
            }
        }
        if (over == shown_.hovered && landing == shown_.landing) return;
        shown_.hovered = over;
        shown_.landing = landing;
        show(mail);
    }
    /// A SHAPE OR A FIELD CARRIED HERE, placed on a label on the canvas or its row: the label shows
    /// it. Placed anywhere else it is refused in words, whatever is selected. Carrying it granted
    /// nothing, and nothing else in the builder changes.
    void on(const ws::PaneCanvasValueDrop& drop, loom::Mail& mail) {
        if (!host(mail, drop.pane) || drop.grant != room_.grant) return;
        try {
            const auto pictured = std::find_if(pictures_.begin(), pictures_.end(),
                                               [&](const auto& p) { return p.content.picture == drop.picture; });
            if (pictured == pictures_.end())
                throw std::invalid_argument("The picture changed while the value was carried; drop it again");
            const auto* hit = pictured->hit(drop.x, drop.y);
            const auto index = hit ? element_of(*hit) : std::nullopt;
            if (!index)
                throw std::invalid_argument("Not shown: drop a shape or a field on a label, on the canvas or in the list");
            const auto value = zengine::inventory::decode_pair(
                std::string_view(reinterpret_cast<const char*>(drop.data.data()), drop.data.size())).item;
            auto [shape, field] = label_source(value);
            if (!shape) throw std::invalid_argument("That description names shapes it does not carry");
            model_.carried(*index, std::move(shape), std::move(field));
            shown_.choices_from = 0;
        } catch (const std::exception& e) {
            model_.notice = e.what();
        }
        shown_.landing.reset();
        show(mail);
    }
    void on(const ws::PaneMenuAnswered& answer, loom::Mail& mail) {
        const auto chosen = menu_.take(mail, answer);
        if (chosen != "carry") return;
        // The subject is still a shape this view says, or nothing is carried.
        const auto intent = std::find_if(model_.description.intents.begin(), model_.description.intents.end(),
                                         [&](const auto& i) { return i.shape->name() == answer.subject; });
        if (intent == model_.description.intents.end()) {
            model_.notice = answer.subject + " is no longer said here; nothing was carried";
            show(mail);
            return;
        }
        carry(intent->shape, mail.correlation(), false, mail);
    }
    void on(const ws::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask()) return;
        if (saving_ && mail.correlation() == saving_->ask) {
            // ALLOWED, THE SAVE WRITES THE FILE NAMED WHEN IT WAS ASKED, and is remembered as any
            // save is, while File still names that file; refused, the refusal is said, and nothing
            // is written or remembered.
            const auto saving = *std::exchange(saving_, std::nullopt);
            if (!answer.allowed) model_.notice = "Saving was refused: " + answer.reason;
            else if (model_.path != saving.path)
                model_.notice = "Not saved: File no longer names " + saving.path + "; save again";
            else perform("save", {saving.path}, mail);
            show(mail);
            return;
        }
        if (!carry_ || mail.correlation() != carry_->ask) return;
        auto carry = std::exchange(carry_, std::nullopt);
        if (!answer.allowed) {
            model_.notice = "Carrying " + carry->label + " was refused: " + answer.reason;
            show(mail);
            return;
        }
        (void)mail.as_role(vb::kRole).send_to_role(
            workshop_role, ws::PaneValueCarryRequested{vb::kPane, carry->label, carry->bytes, carry->drag},
            carry->gesture);
    }
    void on(const ws::PaneCarryAnswered& answer, loom::Mail& mail) {
        if (!answer.carried) {
            model_.notice = "Not carried: " + answer.reason;
            show(mail);
        }
    }
    void on(const vb::ViewEdit& edit, loom::Mail& mail) {
        bool ok = true;
        try {
            pictures_.clear();
            settle();
            effect(model_.command(edit.action, edit.arguments), mail);
            if (edit.action == "new" || edit.action == "open") from_corner();
            edited(edit.action);
        } catch (const std::exception& e) {
            ok = false;
            model_.notice = e.what();
        }
        vb::ViewEdited result{ok, model_.notice, {}};
        try {
            const auto bytes = view::description_bytes(model_.description);
            result.description.assign(bytes.begin(), bytes.end());
        } catch (const std::exception& e) {
            result.ok = false;
            result.reason = e.what();
        }
        (void)mail.answer(result);
        show(mail);
    }
    void on(const view::ViewAnswer& answer, loom::Mail& mail) {
        if (!mail.answers_ask()) return;
        const auto found = pending_.find(mail.correlation());
        if (found == pending_.end() || found->second.action != answer.action) return;
        pending_.erase(found);
        if (answer.ok) {
            model_.running = answer.action != "stop";
            model_.notice = answer.reason;
        } else {
            model_.notice = answer.action + " refused: " + answer.reason;
        }
        remember();
        show(mail);
    }
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!mail.dispatch_refused()) return;
        if (saving_ && saving_->ticket.seq == refused.refused_attempt().seq) {
            saving_.reset();
            model_.notice = "Not saved: Workshop could not be asked to allow it (" + refused.reason + ")";
            show(mail);
            return;
        }
        for (auto it = pending_.begin(); it != pending_.end(); ++it)
            if (it->second.attempt.seq == refused.refused_attempt().seq) {
                model_.notice = it->second.action + " was not delivered: " + refused.reason;
                pending_.erase(it);
                show(mail);
                return;
            }
    }

private:
    static constexpr std::size_t kMaxTyped = 4096;
    struct Pending {
        std::string action;
        loom::Ticket attempt;
    };
    struct Carry {
        std::uint64_t ask = 0, gesture = 0;
        std::string label;
        loom::Bytes bytes;
        bool drag = false;
    };
    /// A SAVE ASKED OF WORKSHOP and not yet answered: the ask's number, its attempt, and the file
    /// File named when it was asked.
    struct Saving {
        std::uint64_t ask = 0;
        loom::Ticket ticket;
        std::string path;
    };
    /// A PRESS THE BUILDER HOLDS while the button is down: a kind dragged from the palette, an
    /// element moved or resized on the design canvas, or the canvas panned. Only a release keeps
    /// what a drag did to the view.
    struct Held {
        enum class What { make, move, resize, pan, size };
        std::int64_t gesture = 0;
        What what = What::make;
        view::Kind kind = view::Kind::label;
        std::size_t element = 0;
        int sx = 0, sy = 0; ///< the sides a handle moves: -1 the left or top, 1 the right or bottom
        std::int64_t x = 0, y = 0;
        view::Element before{};
        bool dirty = false, moved = false;
        vb::Area design{}, placed{}; ///< the pressed picture's design area and the view's room on it
        std::int64_t pan_x = 0, pan_y = 0;
        std::int64_t width = 0, height = 0; ///< the view's size when the press began
    };

    bool host(const loom::Mail& mail, const std::string& which) const {
        return which == vb::kPane && mail.authored_from_role(workshop_role);
    }
    /// The element a place in a picture names: on the canvas, its row, or what it shows or says.
    std::optional<std::size_t> element_of(const vb::Hit& hit) const {
        if (hit.action != "element" && hit.action != "handle" && hit.action != "select" &&
            hit.action != "shows" && hit.action != "says")
            return std::nullopt;
        const auto i = static_cast<std::size_t>(std::stoul(hit.args.at(0)));
        return i < model_.description.elements.size() ? std::optional<std::size_t>(i) : std::nullopt;
    }
    /// Put down what a gesture or a box was doing, for an edit from elsewhere. The pan and the
    /// grid stay.
    void settle() {
        held_.reset();
        vb::Presentation kept;
        kept.pan_x = shown_.pan_x;
        kept.pan_y = shown_.pan_y;
        kept.grid = shown_.grid;
        shown_ = std::move(kept);
    }

    void press(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
        const auto pictured = std::find_if(pictures_.begin(), pictures_.end(),
                                           [&](const auto& p) { return p.content.picture == event.picture; });
        const auto* hit = pictured == pictures_.end() ? nullptr : pictured->hit(event.x, event.y);
        if (event.button == 3) {
            // A RIGHT PRESS ON WHAT A BUTTON SAYS offers to carry its intent's shape; anywhere else
            // it means nothing here, and is handed back for Workshop's pane menu.
            const auto at = hit && hit->action == "says" ? element_of(*hit) : std::nullopt;
            const auto* intent = at ? model_.description.intent(model_.description.elements[*at].id) : nullptr;
            if (!intent) {
                (void)ws::pane_menu::pass_back(mail, vb::kRole, event.pane);
                return;
            }
            menu_ = ws::pane_menu::Offer(vb::kPane, intent->shape->name())
                        .row("carry", "Carry " + intent->shape->name())
                        .send(mail, vb::kRole);
            return;
        }
        if (event.button == 2) {
            // A MIDDLE PRESS ON THE DESIGN CANVAS pans it while the hand holds it.
            if (pictured != pictures_.end() && pictured->design.contains(event.x, event.y, room_.grain))
                held_ = Held{.gesture = event.gesture, .what = Held::What::pan, .x = event.x, .y = event.y,
                             .design = pictured->design, .pan_x = shown_.pan_x,
                             .pan_y = shown_.pan_y};
            return;
        }
        if (event.button != 1) return;
        if (pictured == pictures_.end()) {
            model_.notice = "That picture has changed; choose again.";
            show(mail);
            return;
        }
        if (!hit) return;
        const auto chosen = *hit; // acting clears the pictures the hit points into
        const auto design = pictured->design, placed = pictured->view;
        // A value being typed is kept before anything else is pressed; a refusal keeps its box.
        if (shown_.box && !(chosen.action == "box" && same_box(chosen)) && !commit()) {
            show(mail);
            return;
        }
        if (chosen.action != "new" && chosen.action != "open") shown_.armed.clear();
        const auto index = element_of(chosen);
        if (chosen.action == "box") {
            if (!shown_.box || !same_box(chosen)) focus(chosen, event);
            else place_caret(chosen, event);
        } else if (chosen.action == "kind") {
            held_ = Held{.gesture = event.gesture, .what = Held::What::make, .kind = *view::kind_of(chosen.args.at(0)),
                         .x = event.x, .y = event.y, .dirty = model_.dirty, .design = design, .placed = placed};
        } else if (chosen.action == "element" || chosen.action == "handle") {
            perform("select", {std::to_string(*index)}, mail);
            const bool handle = chosen.action == "handle";
            held_ = Held{.gesture = event.gesture, .what = handle ? Held::What::resize : Held::What::move,
                         .element = *index, .sx = handle ? std::stoi(chosen.args.at(1)) : 0,
                         .sy = handle ? std::stoi(chosen.args.at(2)) : 0, .x = event.x, .y = event.y,
                         .before = model_.description.elements[*index], .dirty = model_.dirty, .design = design,
                         .placed = placed};
        } else if (chosen.action == "size") {
            held_ = Held{.gesture = event.gesture, .what = Held::What::size, .sx = std::stoi(chosen.args.at(0)),
                         .sy = std::stoi(chosen.args.at(1)), .x = event.x, .y = event.y, .dirty = model_.dirty,
                         .design = design, .placed = placed, .width = model_.description.width,
                         .height = model_.description.height};
        } else if (chosen.action == "says") {
            // A PRESS ON WHAT A BUTTON SAYS may drag its intent's shape out: asked under the press,
            // carried only if the hand moves before it lets go.
            perform("select", {std::to_string(*index)}, mail);
            if (const auto* intent = model_.description.intent(model_.description.elements[*index].id))
                carry(intent->shape, mail.correlation(), true, mail);
        } else if (chosen.action == "select" || chosen.action == "shows") {
            perform("select", {std::to_string(*index)}, mail);
        } else if (chosen.action == "canvas") {
            model_.selected.reset();
            model_.choosing.reset();
        } else if (chosen.action == "choices") {
            shown_.choices_from = std::stoul(chosen.args.at(0));
        } else if (chosen.action == "save") {
            ask_save(mail);
        } else {
            perform(chosen.action, chosen.args, mail);
        }
        show(mail);
    }

    /// THE HAND MOVED WHILE THE BUILDER HOLDS ITS PRESS: the canvas pans with it, a kind is shown
    /// where it would be made, an element moves, or a side or a corner resizes it, each snapped
    /// (`vb::snap`). Each is the model's whole edit.
    void drag(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
        auto& h = *held_;
        const auto dx = event.x - h.x, dy = event.y - h.y;
        if (h.what == Held::What::pan) {
            // The view follows the hand, within the pan's reach.
            const auto [reach_x, reach_y] = vb::pan_reach(model_.description, h.design, view::notice_band(room_));
            const auto x = std::clamp<std::int64_t>(h.pan_x - dx, 0, reach_x);
            const auto y = std::clamp<std::int64_t>(h.pan_y - dy, 0, reach_y);
            if (x == shown_.pan_x && y == shown_.pan_y) return;
            shown_.pan_x = x;
            shown_.pan_y = y;
            show(mail);
            return;
        }
        const auto threshold = std::max<std::int64_t>(room_.grain, 4);
        if (!h.moved && std::abs(dx) < threshold && std::abs(dy) < threshold) return;
        h.moved = true;
        if (h.what == Held::What::make) {
            shown_.ghost.reset();
            shown_.met_x.reset();
            shown_.met_y.reset();
            if (h.design.contains(event.x, event.y, room_.grain)) {
                const auto at = made_at(h, event);
                shown_.ghost = vb::Presentation::Ghost{h.kind, at.x, at.y};
                shown_.met_x = at.met_x;
                shown_.met_y = at.met_y;
            }
            show(mail);
            return;
        }
        const auto px = dx, py = dy;
        const bool snapping = !aside(event);
        if (h.what == Held::What::size) {
            const auto most = [](std::int64_t v) { return std::clamp<std::int64_t>(v, 1, view::kMaxSizePx); };
            const auto sized = vb::sized_by_hand(model_.description, most(h.width + (h.sx ? px : 0)),
                                                 most(h.height + (h.sy ? py : 0)), snapping && h.sx != 0,
                                                 snapping && h.sy != 0, shown_.grid);
            const bool met = sized.met_x != shown_.met_x || sized.met_y != shown_.met_y;
            shown_.met_x = sized.met_x;
            shown_.met_y = sized.met_y;
            if (sized.w == model_.description.width && sized.h == model_.description.height) {
                if (met) show(mail);
                return;
            }
            perform("size", {std::to_string(sized.w), std::to_string(sized.h)}, mail);
            show(mail);
            return;
        }
        if (!still_held(h)) {
            held_.reset();
            return;
        }
        const auto& b = h.before;
        vb::Place at{b.x, b.y, b.w, b.h, std::nullopt, std::nullopt};
        auto along_x = vb::Edges::both, along_y = vb::Edges::both;
        // NOTHING SITS OUTSIDE THE VIEW: a dragged element stops at its edges.
        const auto& d = model_.description;
        if (h.what == Held::What::move) {
            at.x = std::clamp<std::int64_t>(b.x + px, 0, std::max<std::int64_t>(0, d.width - b.w));
            at.y = std::clamp<std::int64_t>(b.y + py, 0, std::max<std::int64_t>(0, d.height - b.h));
        } else {
            const auto edges = [](int side) { return side < 0 ? vb::Edges::low : side > 0 ? vb::Edges::high : vb::Edges::none; };
            along_x = edges(h.sx);
            along_y = edges(h.sy);
            if (h.sx > 0) at.w = std::clamp<std::int64_t>(b.w + px, 1, std::max<std::int64_t>(1, d.width - b.x));
            if (h.sx < 0) {
                at.x = std::clamp<std::int64_t>(b.x + px, 0, b.x + b.w - 1);
                at.w = b.x + b.w - at.x;
            }
            if (h.sy > 0) at.h = std::clamp<std::int64_t>(b.h + py, 1, std::max<std::int64_t>(1, d.height - b.y));
            if (h.sy < 0) {
                at.y = std::clamp<std::int64_t>(b.y + py, 0, b.y + b.h - 1);
                at.h = b.y + b.h - at.y;
            }
        }
        const auto snapped = snapping ? vb::snap(model_.description, h.element, at, along_x, along_y, shown_.grid) : at;
        const bool met = snapped.met_x != shown_.met_x || snapped.met_y != shown_.met_y;
        shown_.met_x = snapped.met_x;
        shown_.met_y = snapped.met_y;
        const auto& now = model_.description.elements[h.element];
        if (now.x == snapped.x && now.y == snapped.y && now.w == snapped.w && now.h == snapped.h) {
            if (met) show(mail);
            return;
        }
        perform("place", {std::to_string(h.element), std::to_string(snapped.x), std::to_string(snapped.y),
                          std::to_string(snapped.w), std::to_string(snapped.h)}, mail);
        show(mail);
    }

    /// Alt held while placing by hand sets every snap aside.
    static bool aside(const ws::PaneCanvasPointer& event) { return (event.modifiers & input::mod::kAlt) != 0; }

    /// Where a kind held over the canvas would be made: its corner under the pointer, inside the
    /// view, snapped unless Alt is held.
    vb::Place made_at(const Held& h, const ws::PaneCanvasPointer& event) const {
        const auto [w, height] = vb::made_size(h.kind);
        const auto& d = model_.description;
        const auto at = [](std::int64_t px, std::int64_t most) {
            return std::clamp<std::int64_t>(px, 0, std::max<std::int64_t>(0, most));
        };
        const vb::Place under{at(event.x - h.placed.x, d.width - w), at(event.y - h.placed.y, d.height - height), w,
                              height, std::nullopt, std::nullopt};
        if (aside(event)) return under;
        return vb::snap(model_.description, std::nullopt, under, vb::Edges::both, vb::Edges::both, shown_.grid);
    }

    /// THE PRESS ENDS. A release keeps what the drag did, and a kind let go over the canvas is made
    /// there (a click makes it below the last); a lost press puts back what it moved. A pan stays
    /// where the hand left it.
    void finish(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
        const auto h = *held_;
        held_.reset();
        shown_.ghost.reset();
        shown_.met_x.reset();
        shown_.met_y.reset();
        const bool released = event.phase == ws::canvas_pointer::kRelease;
        if (h.what == Held::What::pan) return;
        if (h.what == Held::What::make) {
            if (released && !h.moved) {
                perform("add", {view::kind_word(h.kind)}, mail);
            } else if (released && h.design.contains(event.x, event.y, room_.grain)) {
                const auto at = made_at(h, event);
                perform("add", {view::kind_word(h.kind), std::to_string(at.x), std::to_string(at.y)}, mail);
            } else if (released) {
                model_.notice = "Let go over the canvas to make a " + std::string(view::kind_word(h.kind)) + " there";
            }
        } else if (h.what == Held::What::size) {
            if (!released && h.moved) {
                perform("size", {std::to_string(h.width), std::to_string(h.height)}, mail);
                model_.dirty = h.dirty;
                model_.notice = "The view's size is back where it was: the drag ended before it was let go";
            }
        } else if (!released && h.moved && still_held(h)) {
            const auto& b = h.before;
            perform("place", {std::to_string(h.element), std::to_string(b.x), std::to_string(b.y),
                              std::to_string(b.w), std::to_string(b.h)}, mail);
            model_.dirty = h.dirty;
            model_.notice = b.id + " is back where it was: the drag ended before it was let go";
        }
        show(mail);
    }

    /// A view made new or opened is shown from its top left corner.
    void from_corner() {
        shown_.pan_x = 0;
        shown_.pan_y = 0;
    }

    /// Is the element a drag holds still the one it pressed? An edit from a key or another
    /// participant may have removed it, and another would then stand at its index.
    bool still_held(const Held& h) const {
        return h.element < model_.description.elements.size() &&
               model_.description.elements[h.element].id == h.before.id;
    }

    /// The wheel over the left column scrolls the element list.
    void scroll(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
        if (pictures_.empty() || event.x >= pictures_.back().design.x || !std::isfinite(event.dy)) return;
        const auto rows = static_cast<std::int64_t>(model_.first_row) - static_cast<std::int64_t>(std::clamp(event.dy, -10.0, 10.0));
        model_.first_row = static_cast<std::size_t>(std::clamp<std::int64_t>(rows, 0, static_cast<std::int64_t>(view::kMaxElements)));
        model_.selected.reset();
        show(mail);
    }

    /// Arrow keys move the selected element a pixel, or a cell with Shift; one that would cross
    /// the view's edge is refused in words.
    bool nudge(const ws::PaneKey& key) {
        const std::int64_t step = (key.modifiers & input::mod::kShift) != 0 ? surface::kCanvasCellPx : 1;
        std::int64_t dx = 0, dy = 0;
        if (key.scancode == input::scan::kLeft) dx = -step;
        else if (key.scancode == input::scan::kRight) dx = step;
        else if (key.scancode == input::scan::kUp) dy = -step;
        else if (key.scancode == input::scan::kDown) dy = step;
        else return false;
        const auto& e = model_.description.elements.at(*model_.selected);
        try {
            model_.command("place", {std::to_string(*model_.selected), std::to_string(e.x + dx), std::to_string(e.y + dy),
                                     std::to_string(e.w), std::to_string(e.h)});
        } catch (const std::exception& error) {
            model_.notice = error.what();
        }
        return true;
    }

    // ---- values in boxes ------------------------------------------------------------------

    bool same_box(const vb::Hit& hit) const {
        const auto element = hit.args.at(1).empty() ? std::optional<std::size_t>()
                                                    : std::optional<std::size_t>(std::stoul(hit.args.at(1)));
        return shown_.box && shown_.box->field == hit.args.at(0) && shown_.box->element == element;
    }
    std::string value_now(const vb::Box& box) const {
        const auto& d = model_.description;
        if (box.field == "name") return d.name;
        if (box.field == "path") return model_.path;
        if (box.field == "width") return std::to_string(d.width);
        if (box.field == "grid") return std::to_string(shown_.grid);
        if (box.field == "height") return std::to_string(d.height);
        if (!box.element || *box.element >= d.elements.size()) return {};
        const auto& e = d.elements[*box.element];
        if (box.field == "intent") {
            const auto* intent = d.intent(e.id);
            return intent ? vb::intent_name(d, *intent) : std::string();
        }
        return vb::value_of(e, box.field);
    }
    void focus(const vb::Hit& hit, const ws::PaneCanvasPointer& event) {
        vb::Box box;
        box.field = hit.args.at(0);
        if (!hit.args.at(1).empty()) box.element = std::stoul(hit.args.at(1));
        const auto value = value_now(box);
        box.text.set(value, value.size());
        shown_.box = std::move(box);
        place_caret(hit, event);
    }
    /// The caret where the box was pressed, at the medium's measured advance, counted from the
    /// scroll the picture drew the box at.
    void place_caret(const vb::Hit& hit, const ws::PaneCanvasPointer& event) {
        const auto metrics = ws::canvas_text_metrics(room_);
        const auto column = (event.x - hit.at.x - metrics.inset) / std::max<std::int64_t>(1, metrics.advance);
        auto& text = shown_.box->text;
        text.place(text.position_at_column(column));
    }
    /// KEEP WHAT WAS TYPED: one whole edit, or the refusal said and the box kept for repair.
    bool commit() {
        auto& box = *shown_.box;
        const auto value = box.text.text();
        if (value == value_now(box)) {
            shown_.box.reset();
            return true;
        }
        try {
            if (box.field == "grid") {
                // THE GRID is the builder's own: set here, never in the view.
                const auto grid = vb::whole(value, "the grid");
                if (grid < 1 || grid > vb::kMaxGrid)
                    throw std::invalid_argument("the grid is whole pixels from 1, which is none, to " +
                                                std::to_string(vb::kMaxGrid) + "; " + value + " is not");
                shown_.grid = grid;
                model_.notice = grid == 1 ? "No grid: a place by hand moves by whole pixels"
                                          : "A place by hand snaps to a grid " + value + " pixels apart";
            } else if (box.field == "name") model_.command("rename", {value});
            else if (box.field == "path") model_.command("path", {value});
            else if (box.field == "width") model_.command("size", {value, std::to_string(model_.description.height)});
            else if (box.field == "height") model_.command("size", {std::to_string(model_.description.width), value});
            else if (box.field == "intent") model_.command("intent", {std::to_string(*box.element), value});
            else model_.command("set", {std::to_string(*box.element), box.field, value});
            shown_.box.reset();
            return true;
        } catch (const std::exception& e) {
            model_.notice = e.what();
            return false;
        }
    }
    /// A key while a box is being typed into: Return keeps the value, Tab keeps it and moves to
    /// the next box, Escape puts it back, and the box's own keys edit it.
    void type_key(const ws::PaneKey& key) {
        if (key.scancode == input::scan::kEscape) {
            shown_.box.reset();
            return;
        }
        if (key.scancode == input::scan::kReturn) {
            (void)commit();
            return;
        }
        if (key.scancode == input::scan::kTab) {
            const auto was = *shown_.box;
            if (!commit()) return;
            const auto next = next_box(was);
            if (!next) return;
            // The next value is selected whole, so what is typed next replaces it.
            const auto value = value_now(*next);
            shown_.box = *next;
            shown_.box->text.set(value, value.size());
            shown_.box->text.select_all();
            return;
        }
        (void)shown_.box->text.consume(key.scancode, key.modifiers, clipboard_);
    }
    /// The box after this one: the view's name, its file, its width, its height and the grid; an
    /// element's values in order.
    std::optional<vb::Box> next_box(const vb::Box& was) const {
        vb::Box next;
        if (!was.element) {
            next.field = was.field == "name"     ? "path"
                         : was.field == "path"   ? "width"
                         : was.field == "width"  ? "height"
                         : was.field == "height" ? "grid"
                                                 : "name";
            return next;
        }
        if (*was.element >= model_.description.elements.size()) return std::nullopt;
        const auto fields = vb::element_fields(model_.description.elements[*was.element]);
        const auto at = std::find(fields.begin(), fields.end(), was.field);
        next.element = was.element;
        next.field = at == fields.end() || at + 1 == fields.end() ? fields.front() : *(at + 1);
        return next;
    }

    // ---- edits and their effects -------------------------------------------------------------

    template <class T> void request(const T& value, const std::string& action, loom::Mail& mail) {
        if (pending_.size() >= 16) throw std::invalid_argument("16 asks are unanswered; wait for the view host");
        const auto correlation = ++correlation_;
        const auto ticket = mail.as_role(vb::kRole).send_to_role(view::kViewHostRole, value, correlation);
        if (!ticket.valid()) throw std::invalid_argument(action + " could not be queued");
        pending_.emplace(correlation, Pending{action, ticket});
        model_.notice = action + " asked of the view host";
    }
    void effect(const vb::Action& action, loom::Mail& mail) {
        switch (action.effect) {
        case vb::Effect::None: return;
        case vb::Effect::Run: request(view::ViewRun{"builder", action.payload}, "run", mail); return;
        case vb::Effect::Apply: request(view::ViewApply{"builder", action.payload}, "apply", mail); return;
        case vb::Effect::Stop: request(view::ViewStop{"builder"}, "stop", mail); return;
        }
    }
    /// ASK TO CARRY AN INTENT'S SHAPE OUT as a description holding what it nests, under the
    /// gesture it continues: a press's drag, placed where it is released, or a menu choice's carry,
    /// placed by a click.
    void carry(const std::shared_ptr<const loom::Schema>& shape, std::uint64_t gesture, bool drag, loom::Mail& mail) {
        const auto bytes = zengine::inventory::encode_pair(zengine::flow::shape::carried(shape), {});
        carry_ = Carry{++correlation_, gesture, shape->name(), loom::Bytes(bytes.begin(), bytes.end()), drag};
        (void)mail.as_role(vb::kRole).send_to_role(
            workshop_role,
            ws::PaneOperationRequested{vb::kPane, workshop_role, ws::PaneValueCarryRequested::zen_name, 1,
                                       static_cast<std::int64_t>(carry_->gesture)},
            carry_->ask);
    }
    /// ASK BEFORE A HAND'S SAVE WRITES: a save is class `write`, judged for the actor of the
    /// gesture that delivered it, a key's row or a press, and that gesture's approval is spent on
    /// this save alone. Nothing is written until Workshop allows it, and one save waits at a time.
    /// A File naming nothing is said at once, with nothing asked.
    void ask_save(loom::Mail& mail) {
        if (saving_) {
            model_.notice = "A save is waiting for Workshop's answer; save again once it comes";
            return;
        }
        if (model_.path.empty()) {
            perform("save", {}, mail);
            return;
        }
        Saving asked{++correlation_, {}, model_.path};
        asked.ticket = mail.as_role(vb::kRole).send_to_role(
            workshop_role,
            ws::v2::PaneOperationRequested{vb::kPane, {}, {}, 0, static_cast<std::int64_t>(mail.correlation()),
                                           {ws::scope::kWrite}, {}},
            asked.ask);
        if (!asked.ticket.valid()) {
            model_.notice = "Not saved: Workshop could not be asked to allow it";
            return;
        }
        saving_ = std::move(asked);
    }
    void offer(loom::Mail& mail) {
        (void)mail.as_role(vb::kRole).send_to_role(
            workshop_role, ws::v3::PaneOffered{vb::kPane, "View Builder", "make a view by hand, then run it beside Flow", 880, 540, 0});
        (void)mail.as_role(vb::kRole).send_to_role(
            workshop_role, ws::PaneActions{vb::kPane,
                                           {{"save", "Save the view", input::scan::kS, input::mod::kCtrl},
                                            {"open", "Open a view", input::scan::kO, input::mod::kCtrl},
                                            {"run", "Run the view", input::scan::kR, input::mod::kCtrl}}});
    }
    static std::string identifier_of(const std::string& label) {
        std::string out;
        for (char c : label)
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') out += c;
        return out.empty() || std::isdigit(static_cast<unsigned char>(out.front())) ? "Said" : out;
    }
    /// One control's edit. A New or an Open over an unsaved view waits for its second press.
    void perform(const std::string& action, const std::vector<std::string>& args, loom::Mail& mail) {
        try {
            pictures_.clear();
            if ((action == "new" || action == "open") && args.empty()) {
                if (model_.dirty && shown_.armed != action) {
                    shown_.armed = action;
                    model_.notice = "The view is unsaved: Save it, or press " +
                                    std::string(action == "new" ? "New" : "Open") + " again to discard it";
                    return;
                }
                shown_.armed.clear();
                if (action == "open" && model_.path.empty()) {
                    model_.notice = "Type the view's file under File, then Open";
                    return;
                }
                if (action == "new") effect(model_.command("new", {"my.view", "discard"}), mail);
                else effect(model_.command("open", {model_.path, "discard"}), mail);
                from_corner();
                edited(action);
                return;
            }
            if (action == "intent" && args.size() == 1) {
                const auto& e = model_.description.elements.at(std::stoul(args[0]));
                const auto* had = model_.description.intent(e.id);
                effect(model_.command("intent", {args[0], had ? vb::intent_name(model_.description, *had) : identifier_of(e.label)}), mail);
                return;
            }
            effect(model_.command(action, args), mail);
            edited(action);
            if (action != "select") {
                shown_.hovered.reset();
                shown_.landing.reset();
            }
        } catch (const std::exception& e) {
            model_.notice = e.what();
        }
    }
    /// AN EDIT THAT WAS TAKEN: an open or a save names the file the draft is, a new names none, and
    /// each makes the project file the builder's to write again.
    void edited(const std::string& action) {
        if (action == "open" || action == "save" || action == "new") {
            file_ = action == "new" ? std::string() : model_.path;
            kept_ = false;
        }
        remember();
    }
    /// A path as this builder's project file and its Open mean it: relative to the project.
    std::string in_project(const std::string& path) const {
        const std::filesystem::path p(path);
        return p.is_absolute() || project_dir_.empty() ? path : (std::filesystem::path(project_dir_) / p).generic_string();
    }
    /// ...AND A PATH AS THE PROJECT FILE KEEPS IT: a file inside the project relative to it, so a
    /// project moved whole still names its view; any other path as it is.
    std::string project_relative(const std::string& path) const {
        const std::filesystem::path p(path);
        if (project_dir_.empty() || !p.is_absolute()) return path;
        const std::filesystem::path inside = p.lexically_relative(std::filesystem::path(project_dir_));
        if (inside.empty() || *inside.begin() == "..") return path;
        return inside.generic_string();
    }
    /// KEEP, IN THE PROJECT, THE FILE THE DRAFT IS AND WHETHER ITS VIEW RUNS, when either differs
    /// from what the project file holds. What it holds is counted only once the write succeeds, so
    /// a write that failed is made again at the next chance. A project file naming a view this
    /// builder could not open at a launch stands as it is until the weaver opens, saves or starts
    /// a view, so the file's return brings that view back.
    void remember() {
        if (project_dir_.empty() || kept_) return;
        const std::pair<std::string, bool> now{project_relative(file_), model_.running};
        if (remembered_ == now) return;
        const vb::ViewBuilderRun record{vb::kRunFormat, vb::kRunFormatVersion, now.first, now.second};
        const auto error = zengine::maker::write_file(in_project(vb::kRunFileName), loom::compat::serialize(loom::to_value(record)));
        if (!error.empty()) {
            model_.notice += " (not remembered for the next launch: " + error + ")";
            return;
        }
        remembered_ = now;
    }
    /// WHAT THE PROJECT FILE HOLDS: the view file it names and whether its view ran -- nothing named
    /// when there is no file -- or none, `why` saying so, when this builder cannot read it.
    std::optional<std::pair<std::string, bool>> recorded(std::string& why) const {
        std::error_code ec;
        const auto record_path = in_project(vb::kRunFileName);
        if (!std::filesystem::exists(record_path, ec)) return std::make_pair(std::string(), false);
        const auto read = zengine::maker::read_file(record_path);
        if (!read) {
            why = read.reason;
            return std::nullopt;
        }
        const loom::Unverified claim = loom::compat::parse(read.bytes);
        auto admitted = loom::admit(claim, loom::schema_of<vb::ViewBuilderRun>());
        if (!admitted) {
            why = std::string(vb::kRunFileName) + ": " + admitted.first_error().message();
            return std::nullopt;
        }
        const auto record = loom::from_value<vb::ViewBuilderRun>(admitted.value());
        if (record.format != vb::kRunFormat || record.format_version != vb::kRunFormatVersion) {
            why = std::string(vb::kRunFileName) + " is not this builder's file";
            return std::nullopt;
        }
        return std::make_pair(record.path, record.running);
    }
    /// A LAUNCH: open the view file the project file names and, if its view ran, run it again with
    /// its pane asking nothing -- the desk the weaver left seats it. A file it cannot open leaves
    /// the project file as it found it.
    void resume(loom::Mail& mail) {
        if (!remembered_ || remembered_->first.empty()) return;
        const auto [path, run] = *remembered_;
        bool opened = false;
        try {
            effect(model_.command("open", {in_project(path), "discard"}), mail);
            opened = true;
            from_corner();
            file_ = model_.path;
            if (!run) {
                remember();
                return;
            }
            const auto bytes = view::description_bytes(model_.description);
            request(view::ViewResume{"builder", loom::Bytes(bytes.begin(), bytes.end())}, "resume", mail);
        } catch (const std::exception& e) {
            // SAID FOR WHAT WAS ASKED: a stopped view's file was only to be opened.
            model_.notice =
                (run ? "Could not run " + path + " again: " : "Could not open " + path + ": ") + e.what();
            kept_ = !opened;
        }
    }
    void show(loom::Mail& mail) {
        if (room_.grant > 0 && room_.width > 0 && room_.height > 0) {
            auto current = vb::picture(model_, shown_, room_, ++picture_number_);
            const auto ticket =
                mail.as_role(vb::kRole).send_to_role(workshop_role, vb::named(current, model_.description));
            if (ticket.valid()) {
                pictures_.push_back(std::move(current));
                while (pictures_.size() > 8) pictures_.pop_front();
            }
        } else if (rows_ > 0 && columns_ > 0) {
            std::string row = "View Builder: making a view needs a canvas-capable Workshop";
            if (static_cast<std::int64_t>(row.size()) > columns_) row.resize(static_cast<std::size_t>(columns_));
            (void)mail.as_role(vb::kRole).send_to_role(workshop_role, ws::PaneContent{vb::kPane, {{row}}});
        }
    }

    vb::Model model_;
    vb::Presentation shown_;
    std::optional<Held> held_;
    zengine::ActivationCursor activation_;
    bool restored_ = false;
    /// WHERE THE PROJECT IS, as `zengine.project` answered the ask numbered `root_ask_`; whether
    /// that answer is a launch's; the file the draft was saved to or opened from, never a name only
    /// typed into File; what the project file holds, as last read or written, none while that is
    /// not known; and whether it stands as a launch found it, naming a view file this builder
    /// could not open.
    std::string project_dir_;
    std::uint64_t root_ask_ = 0;
    bool launch_ = false;
    std::string file_;
    std::optional<std::pair<std::string, bool>> remembered_;
    bool kept_ = false;
    ws::PaneCanvasRoom room_;
    std::int64_t rows_ = 0, columns_ = 0, picture_number_ = 0;
    std::uint64_t correlation_ = 0;
    std::map<std::uint64_t, Pending> pending_;
    std::deque<vb::Picture> pictures_;
    zengine::component::Clipboard clipboard_;
    ws::pane_menu::Asked menu_;
    std::optional<Carry> carry_;
    std::optional<Saving> saving_;
};
} // namespace
ZEN_EXPORT_WEAVE(ViewBuilderPane)
