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
#include "workshop/pane_carry.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_operation.hpp"
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
#include <map>
#include <string>

namespace {
namespace vb = zengine::view_builder;
namespace view = zengine::view;
namespace ws = zengine::workshop;
namespace input = zengine::input;
namespace surface = zengine::surface;
constexpr const char* workshop_role = "zengine.workshop";

/// The shape a carried value names, and the field when the carry was one field: a description's
/// shape, a field's root shape and its top-level name, or a value's own shape.
std::pair<std::shared_ptr<const loom::Schema>, std::optional<std::string>> carried_shape(const loom::Value& value) {
    if (loom::same_identity(value.schema(), *loom::schema_desc_schema())) {
        loom::Registry none;
        return {loom::decode_schema(value, none), std::nullopt};
    }
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

/// Subunits as whole pixels, floored: a terminal's cell is twelve of them.
std::int64_t pixels(std::int64_t subs) { return surface::floor_div_px(subs, surface::kPixelGrainSubs); }

class ViewBuilderPane final
    : public loom::WeaveBase<
          ViewBuilderPane, vb::BuilderState,
          loom::Accept<loom::Activated, ws::PaneCatalogRequested, ws::PaneRoom, ws::PaneCanvasRoom,
                       ws::PaneCanvasPointer, ws::PaneCanvasHover, ws::PaneCanvasRejected, ws::PaneKey,
                       ws::PaneTextInput, ws::PaneActionRequested, ws::ActionsJudged, ws::PaneQuitRequested,
                       ws::PaneCanvasValueDrop, ws::PaneMenuAnswered, ws::PaneOperationAnswered,
                       ws::PaneCarryAnswered, vb::ViewEdit, view::ViewAnswer, loom::DispatchRefused>,
          loom::Emit<ws::v2::PaneOffered, ws::PaneContent, ws::PaneCanvasContent, ws::PaneActions,
                     ws::PaneEscapeUnspent, ws::PanePassRequested, ws::PaneMenuRequested,
                     ws::PaneQuitAnswered, ws::PaneOperationRequested, ws::PaneValueCarryRequested,
                     vb::ViewEdited, view::ViewRun, view::ViewApply, view::ViewStop>> {
public:
    loom::Value snapshot() const override {
        vb::BuilderState saved;
        try {
            const auto bytes = view::description_bytes(model_.description);
            saved.description.assign(bytes.begin(), bytes.end());
        } catch (const std::exception&) {
        }
        saved.path = model_.path;
        saved.dirty = model_.dirty;
        saved.running = model_.running;
        return loom::to_value(saved);
    }

    void on(const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) return;
        if (!restored_) {
            restored_ = true;
            if (!state_.description.empty()) {
                auto read = view::read_description(std::string_view(
                    reinterpret_cast<const char*>(state_.description.data()), state_.description.size()));
                if (read) {
                    model_.description = std::move(read.description);
                    model_.path = state_.path;
                    model_.dirty = state_.dirty;
                    model_.running = state_.running;
                } else {
                    model_.notice = "Could not restore the View Builder: " + read.reason;
                }
            }
        }
        offer(mail);
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
        // A fresh room ends whatever the old one held: a drag, a hover, a mark.
        room_ = room;
        pictures_.clear();
        held_.reset();
        shown_.hovered.reset();
        shown_.landing.reset();
        shown_.ghost.reset();
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
        perform(action.id, {}, mail);
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
    /// it. Carrying it granted nothing, and nothing else in the builder changes.
    void on(const ws::PaneCanvasValueDrop& drop, loom::Mail& mail) {
        if (!host(mail, drop.pane) || drop.grant != room_.grant) return;
        try {
            const auto pictured = std::find_if(pictures_.begin(), pictures_.end(),
                                               [&](const auto& p) { return p.content.picture == drop.picture; });
            if (pictured == pictures_.end())
                throw std::invalid_argument("The picture changed while the value was carried; drop it again");
            const auto* hit = pictured->hit(drop.x, drop.y);
            std::optional<std::size_t> index = hit ? element_of(*hit) : std::nullopt;
            if (!index) index = model_.selected;
            if (!index) throw std::invalid_argument("Drop a shape or a field on a label");
            const auto value = zengine::inventory::decode_pair(
                std::string_view(reinterpret_cast<const char*>(drop.data.data()), drop.data.size())).item;
            auto [shape, field] = carried_shape(value);
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
        carry(*intent->shape, mail.correlation(), false, mail);
    }
    void on(const ws::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (!carry_ || !mail.answers_ask() || mail.correlation() != carry_->ask) return;
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
        show(mail);
    }
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!mail.dispatch_refused()) return;
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
    /// A PRESS THE BUILDER HOLDS while the button is down: a kind dragged from the palette, or an
    /// element moved or resized on the design canvas. Only a release keeps what it did.
    struct Held {
        enum class What { make, move, resize };
        std::int64_t gesture = 0;
        What what = What::make;
        view::Kind kind = view::Kind::label;
        std::size_t element = 0;
        int corner = 3;
        std::int64_t x = 0, y = 0;
        view::Element before;
        bool dirty = false, moved = false;
        vb::Area design;
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
    /// Put down what a gesture or a box was doing, for an edit from elsewhere.
    void settle() {
        held_.reset();
        shown_ = vb::Presentation{};
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
        if (event.button != 1) return;
        if (pictured == pictures_.end()) {
            model_.notice = "That picture has changed; choose again.";
            show(mail);
            return;
        }
        if (!hit) return;
        const auto chosen = *hit; // acting clears the pictures the hit points into
        const auto design = pictured->design;
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
            held_ = Held{event.gesture, Held::What::make, *view::kind_of(chosen.args.at(0)), 0, 3,
                         event.x, event.y, {}, model_.dirty, false, design};
        } else if (chosen.action == "element" || chosen.action == "handle") {
            perform("select", {std::to_string(*index)}, mail);
            const auto what = chosen.action == "handle" ? Held::What::resize : Held::What::move;
            const int corner = chosen.action == "handle" ? std::stoi(chosen.args.at(1)) : 3;
            held_ = Held{event.gesture, what, view::Kind::label, *index, corner, event.x, event.y,
                         model_.description.elements[*index], model_.dirty, false, design};
        } else if (chosen.action == "says") {
            // A PRESS ON WHAT A BUTTON SAYS may drag its intent's shape out: asked under the press,
            // carried only if the hand moves before it lets go.
            perform("select", {std::to_string(*index)}, mail);
            if (const auto* intent = model_.description.intent(model_.description.elements[*index].id))
                carry(*intent->shape, mail.correlation(), true, mail);
        } else if (chosen.action == "select" || chosen.action == "shows") {
            perform("select", {std::to_string(*index)}, mail);
        } else if (chosen.action == "canvas") {
            model_.selected.reset();
            model_.choosing.reset();
        } else if (chosen.action == "choices") {
            shown_.choices_from = std::stoul(chosen.args.at(0));
        } else {
            perform(chosen.action, chosen.args, mail);
        }
        show(mail);
    }

    /// THE HAND MOVED WHILE THE BUILDER HOLDS ITS PRESS, in whole pixels: a kind is shown where it
    /// would be made, an element moves, or a corner resizes it. Each is the model's whole edit.
    void drag(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
        auto& h = *held_;
        const auto dx = event.x - h.x, dy = event.y - h.y;
        const auto threshold = std::max<std::int64_t>(room_.grain, surface::subs_of_pixel(4));
        if (!h.moved && std::abs(dx) < threshold && std::abs(dy) < threshold) return;
        h.moved = true;
        const auto bound = [](std::int64_t v, std::int64_t low) { return std::clamp<std::int64_t>(v, low, view::kMaxPixels); };
        if (h.what == Held::What::make) {
            if (h.design.contains(event.x, event.y, room_.grain))
                shown_.ghost = vb::Presentation::Ghost{h.kind, bound(pixels(event.x - h.design.x), 0),
                                                       bound(pixels(event.y - h.design.y), 0)};
            else
                shown_.ghost.reset();
            show(mail);
            return;
        }
        if (!still_held(h)) {
            held_.reset();
            return;
        }
        const auto& b = h.before;
        auto x = b.x, y = b.y, w = b.w, h_ = b.h;
        const auto px = pixels(dx), py = pixels(dy);
        if (h.what == Held::What::move) {
            x = bound(b.x + px, 0);
            y = bound(b.y + py, 0);
        } else {
            if (h.corner & 1) w = bound(b.w + px, 1);
            else {
                x = std::clamp<std::int64_t>(b.x + px, 0, b.x + b.w - 1);
                w = b.x + b.w - x;
            }
            if (h.corner & 2) h_ = bound(b.h + py, 1);
            else {
                y = std::clamp<std::int64_t>(b.y + py, 0, b.y + b.h - 1);
                h_ = b.y + b.h - y;
            }
        }
        const auto& now = model_.description.elements[h.element];
        if (now.x == x && now.y == y && now.w == w && now.h == h_) return;
        perform("place", {std::to_string(h.element), std::to_string(x), std::to_string(y), std::to_string(w),
                          std::to_string(h_)}, mail);
        show(mail);
    }

    /// THE PRESS ENDS. A release keeps what the drag did, and a kind let go over the canvas is made
    /// there (a click makes it below the last); a lost press puts back what it moved.
    void finish(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
        const auto h = *held_;
        held_.reset();
        shown_.ghost.reset();
        const bool released = event.phase == ws::canvas_pointer::kRelease;
        if (h.what == Held::What::make) {
            if (released && !h.moved) {
                perform("add", {view::kind_word(h.kind)}, mail);
            } else if (released && h.design.contains(event.x, event.y, room_.grain)) {
                perform("add", {view::kind_word(h.kind), std::to_string(std::max<std::int64_t>(0, pixels(event.x - h.design.x))),
                                std::to_string(std::max<std::int64_t>(0, pixels(event.y - h.design.y)))}, mail);
            } else if (released) {
                model_.notice = "Let go over the canvas to make a " + std::string(view::kind_word(h.kind)) + " there";
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

    /// Arrow keys move the selected element a pixel, or a cell with Shift.
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
            model_.command("place", {std::to_string(*model_.selected), std::to_string(std::max<std::int64_t>(0, e.x + dx)),
                                     std::to_string(std::max<std::int64_t>(0, e.y + dy)), std::to_string(e.w),
                                     std::to_string(e.h)});
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
            if (box.field == "name") model_.command("rename", {value});
            else if (box.field == "path") model_.command("path", {value});
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
    /// The box after this one: the view's name, then its file; an element's values in order.
    std::optional<vb::Box> next_box(const vb::Box& was) const {
        vb::Box next;
        if (!was.element) {
            next.field = was.field == "name" ? "path" : "name";
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
    /// ASK TO CARRY AN INTENT'S SHAPE OUT as a description, under the gesture it continues: a
    /// press's drag, placed where it is released, or a menu choice's carry, placed by a click.
    void carry(const loom::Schema& shape, std::uint64_t gesture, bool drag, loom::Mail& mail) {
        const auto bytes = zengine::inventory::encode_pair(loom::encode_schema(shape), {});
        carry_ = Carry{++correlation_, gesture, shape.name(), loom::Bytes(bytes.begin(), bytes.end()), drag};
        (void)mail.as_role(vb::kRole).send_to_role(
            workshop_role,
            ws::PaneOperationRequested{vb::kPane, workshop_role, ws::PaneValueCarryRequested::zen_name, 1,
                                       static_cast<std::int64_t>(carry_->gesture)},
            carry_->ask);
    }
    void offer(loom::Mail& mail) {
        (void)mail.as_role(vb::kRole).send_to_role(
            workshop_role, ws::v2::PaneOffered{vb::kPane, "View Builder", "make a view by hand, then run it beside Flow", 30, 110});
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
                if (action == "new") effect(model_.command("new", {"my.view", "discard"}), mail);
                else if (model_.path.empty()) model_.notice = "Type the view's file under File, then Open";
                else effect(model_.command("open", {model_.path, "discard"}), mail);
                return;
            }
            if (action == "intent" && args.size() == 1) {
                const auto& e = model_.description.elements.at(std::stoul(args[0]));
                const auto* had = model_.description.intent(e.id);
                effect(model_.command("intent", {args[0], had ? vb::intent_name(model_.description, *had) : identifier_of(e.label)}), mail);
                return;
            }
            effect(model_.command(action, args), mail);
            if (action != "select") {
                shown_.hovered.reset();
                shown_.landing.reset();
            }
        } catch (const std::exception& e) {
            model_.notice = e.what();
        }
    }
    void show(loom::Mail& mail) {
        if (room_.grant > 0 && room_.width > 0 && room_.height > 0) {
            auto current = vb::picture(model_, shown_, room_, ++picture_number_);
            const auto ticket = mail.as_role(vb::kRole).send_to_role(workshop_role, current.content);
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
    ws::PaneCanvasRoom room_;
    std::int64_t rows_ = 0, columns_ = 0, picture_number_ = 0;
    std::uint64_t correlation_ = 0;
    std::map<std::uint64_t, Pending> pending_;
    std::deque<vb::Picture> pictures_;
    zengine::component::Clipboard clipboard_;
    ws::pane_menu::Asked menu_;
    std::optional<Carry> carry_;
};
} // namespace
ZEN_EXPORT_WEAVE(ViewBuilderPane)
