// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
// The View Builder: a pane beside Flow that edits one view description and runs it through the
// view host. It draws its own lists and never the view. Law: agents/view.md. Guide:
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
#include <deque>
#include <map>
#include <string>

namespace {
namespace vb = zengine::view_builder;
namespace view = zengine::view;
namespace ws = zengine::workshop;
namespace input = zengine::input;
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

class ViewBuilderPane final
    : public loom::WeaveBase<
          ViewBuilderPane, vb::BuilderState,
          loom::Accept<loom::Activated, ws::PaneCatalogRequested, ws::PaneRoom, ws::PaneCanvasRoom,
                       ws::PaneCanvasPointer, ws::PaneCanvasRejected, ws::PaneKey, ws::PaneTextInput,
                       ws::PaneActionRequested, ws::ActionsJudged, ws::PaneQuitRequested,
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
        room_ = room;
        pictures_.clear();
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
        if (host(mail, action.pane)) perform(action.id, {}, mail);
    }
    void on(const ws::PaneQuitRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(workshop_role)) return;
        const std::string why = !pending_.empty() ? "The View Builder is waiting for the view host. Quit again after it answers."
                                : model_.dialog   ? "The View Builder has an unfinished dialog. Confirm or cancel it before quitting."
                                : model_.dirty    ? "The View Builder has an unsaved view. Save it before quitting."
                                                  : "";
        (void)mail.answer(ws::PaneQuitAnswered{vb::kPane, why.empty(), why});
    }
    void on(const ws::PaneKey& key, loom::Mail& mail) {
        if (!host(mail, key.pane)) return;
        if (model_.dialog) {
            if (key.scancode == input::scan::kEscape) {
                model_.dialog.reset();
            } else if (key.scancode == input::scan::kReturn) {
                perform("dialog-confirm", {}, mail);
                return;
            } else if (key.scancode == input::scan::kTab) {
                auto& d = *model_.dialog;
                d.selected = (d.selected + 1) % d.entries.size();
            } else {
                model_.dialog->entries.at(model_.dialog->selected).text.consume(key.scancode, key.modifiers, clipboard_);
            }
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
            show(mail);
        } else if (key.scancode == input::scan::kDelete && model_.selected) {
            perform("remove", {std::to_string(*model_.selected)}, mail);
        }
    }
    void on(const ws::PaneTextInput& text, loom::Mail& mail) {
        if (!host(mail, text.pane) || !model_.dialog) return;
        auto& box = model_.dialog->entries.at(model_.dialog->selected).text;
        if (box.size() + text.text.size() > 4096) model_.notice = "Field editing is limited to 4096 bytes";
        else box.type(text.text);
        show(mail);
    }
    void on(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
        if (!host(mail, event.pane) || event.grant != room_.grant || event.phase != ws::canvas_pointer::kPress) return;
        const auto pictured = std::find_if(pictures_.begin(), pictures_.end(),
                                           [&](const auto& p) { return p.content.picture == event.picture; });
        const auto* hit = pictured == pictures_.end() ? nullptr : pictured->hit(event.x, event.y);
        if (event.button == 3) {
            // A RIGHT PRESS ON WHAT A BUTTON SAYS offers to carry its intent's shape; anywhere else
            // it means nothing here, and is handed back for Workshop's pane menu.
            const auto* intent = hit && hit->action == "says" ? intent_at(hit->args.at(0)) : nullptr;
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
        perform(chosen.action, chosen.args, mail);
    }
    /// A SHAPE OR A FIELD CARRIED HERE, placed on a label's row: the label shows it. Carrying it
    /// granted nothing, and nothing else in the builder changes.
    void on(const ws::PaneCanvasValueDrop& drop, loom::Mail& mail) {
        if (!host(mail, drop.pane) || drop.grant != room_.grant) return;
        try {
            if (model_.dialog) throw std::invalid_argument("Confirm or cancel the open dialog before dropping a value");
            const auto pictured = std::find_if(pictures_.begin(), pictures_.end(),
                                               [&](const auto& p) { return p.content.picture == drop.picture; });
            if (pictured == pictures_.end())
                throw std::invalid_argument("The picture changed while the value was carried; drop it again");
            const auto* hit = pictured->hit(drop.x, drop.y);
            std::optional<std::size_t> index;
            if (hit && (hit->action == "select" || hit->action == "shows")) index = std::stoul(hit->args.at(0));
            else if (model_.selected) index = model_.selected;
            if (!index) throw std::invalid_argument("Drop a shape or a field on a label");
            const auto value = zengine::inventory::decode_pair(
                std::string_view(reinterpret_cast<const char*>(drop.data.data()), drop.data.size())).item;
            auto [shape, field] = carried_shape(value);
            if (!shape) throw std::invalid_argument("That description names shapes it does not carry");
            model_.carried(*index, std::move(shape), std::move(field));
        } catch (const std::exception& e) {
            model_.notice = e.what();
        }
        pictures_.clear();
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
        const auto bytes = zengine::inventory::encode_pair(loom::encode_schema(*intent->shape), {});
        carry_ = Carry{++correlation_, mail.correlation(), intent->shape->name(), loom::Bytes(bytes.begin(), bytes.end())};
        (void)mail.as_role(vb::kRole).send_to_role(
            workshop_role,
            ws::PaneOperationRequested{vb::kPane, workshop_role, ws::PaneValueCarryRequested::zen_name, 1,
                                       static_cast<std::int64_t>(carry_->gesture)},
            carry_->ask);
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
            workshop_role, ws::PaneValueCarryRequested{vb::kPane, carry->label, carry->bytes, false}, carry->gesture);
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
            if (model_.dialog) throw std::invalid_argument("Confirm or cancel the open dialog before another edit");
            pictures_.clear();
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
    struct Pending {
        std::string action;
        loom::Ticket attempt;
    };
    struct Carry {
        std::uint64_t ask = 0, gesture = 0;
        std::string label;
        loom::Bytes bytes;
    };
    bool host(const loom::Mail& mail, const std::string& which) const {
        return which == vb::kPane && mail.authored_from_role(workshop_role);
    }
    const view::Intent* intent_at(const std::string& index) const {
        const auto i = std::stoul(index);
        if (i >= model_.description.elements.size()) return nullptr;
        return model_.description.intent(model_.description.elements[i].id);
    }
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
    void offer(loom::Mail& mail) {
        (void)mail.as_role(vb::kRole).send_to_role(
            workshop_role, ws::v2::PaneOffered{vb::kPane, "View Builder", "describe a view, then run it beside Flow", 24, 72});
        (void)mail.as_role(vb::kRole).send_to_role(
            workshop_role, ws::PaneActions{vb::kPane,
                                           {{"ask-save", "Save the view", input::scan::kS, input::mod::kCtrl},
                                            {"ask-open", "Open a view", input::scan::kO, input::mod::kCtrl},
                                            {"run", "Run the view", input::scan::kR, input::mod::kCtrl}}});
    }
    void perform(const std::string& action, const std::vector<std::string>& args, loom::Mail& mail) {
        try {
            act(action, args);
            effect(pending_effect_, mail);
        } catch (const std::exception& e) {
            model_.notice = e.what();
        }
        pending_effect_ = {};
        show(mail);
    }
    static std::string identifier_of(const std::string& label) {
        std::string out;
        for (char c : label)
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') out += c;
        return out.empty() || std::isdigit(static_cast<unsigned char>(out.front())) ? "Said" : out;
    }
    void act(const std::string& action, const std::vector<std::string>& args) {
        if (model_.dialog && action != "dialog-field" && action != "dialog-confirm" && action != "dialog-cancel")
            throw std::invalid_argument("Confirm or cancel the open dialog before another edit");
        pictures_.clear();
        const auto& d = model_.description;
        if (action == "ask-new") {
            model_.ask("New view (replaces the draft)", "new", {{"Name", "my.view"}, {"Unsaved disposition", "discard"}});
        } else if (action == "ask-open") {
            model_.ask("Open a view description", "open", {{"Path", model_.path}, {"Unsaved disposition", "discard"}});
        } else if (action == "ask-save") {
            model_.ask("Save the view description", "save", {{"Path", model_.path.empty() ? "my.view" : model_.path}});
        } else if (action == "ask-rename") {
            model_.ask("Rename the view: its office, its pane and its intents' namespace", "rename", {{"Name", d.name}});
        } else if (action == "ask-element") {
            const auto i = std::stoul(args.at(0));
            const auto& e = d.elements.at(i);
            std::vector<std::pair<std::string, std::string>> fields{
                {"Id", e.id}, {"Label", e.label}, {"X px", std::to_string(e.x)}, {"Y px", std::to_string(e.y)},
                {"Width px", std::to_string(e.w)}, {"Height px", std::to_string(e.h)}};
            if (e.kind == view::Kind::number) fields.push_back({"Starting text", e.text});
            model_.ask("Edit " + e.id + ", in whole pixels, " + std::to_string(zengine::surface::kCanvasCellPx) +
                           " to a cell",
                       "element:" + args.at(0), fields);
        } else if (action == "ask-intent") {
            const auto& e = d.elements.at(std::stoul(args.at(0)));
            model_.ask("Make an intent from the view's number fields, said by " + e.id, "intent:" + args.at(0),
                       {{"Intent name (inside " + d.name + ")", identifier_of(e.label)}});
        } else if (action == "dialog-field") {
            model_.dialog->selected = std::stoul(args.at(0));
        } else if (action == "dialog-cancel") {
            model_.dialog.reset();
        } else if (action == "dialog-confirm") {
            if (!model_.dialog) return;
            const auto asked = *model_.dialog;
            std::vector<std::string> values;
            for (const auto& entry : asked.entries) values.push_back(entry.text.text());
            const auto colon = asked.action.find(':');
            const auto verb = asked.action.substr(0, colon);
            if (colon != std::string::npos) values.insert(values.begin(), asked.action.substr(colon + 1));
            // A refusal keeps the dialog and its text for repair.
            pending_effect_ = model_.command(verb, values);
            model_.dialog.reset();
        } else if (action == "says" || action == "shows") {
            model_.command("select", args);
        } else {
            pending_effect_ = model_.command(action, args);
        }
    }
    void show(loom::Mail& mail) {
        if (room_.grant > 0 && room_.width > 0 && room_.height > 0) {
            auto current = vb::picture(model_, room_, ++picture_number_);
            const auto ticket = mail.as_role(vb::kRole).send_to_role(workshop_role, current.content);
            if (ticket.valid()) {
                pictures_.push_back(std::move(current));
                while (pictures_.size() > 8) pictures_.pop_front();
            }
        } else if (rows_ > 0 && columns_ > 0) {
            std::string row = "View Builder: describing a view needs a canvas-capable Workshop";
            if (static_cast<std::int64_t>(row.size()) > columns_) row.resize(static_cast<std::size_t>(columns_));
            (void)mail.as_role(vb::kRole).send_to_role(workshop_role, ws::PaneContent{vb::kPane, {{row}}});
        }
    }

    vb::Model model_;
    vb::Action pending_effect_;
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
