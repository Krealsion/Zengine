// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "activation/activation.hpp"
#include "flow-host/vocabulary.hpp"
#include "flow-pane/view.hpp"
#include "flow-pane/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "operator/host.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"
#include <cmath>
#include <deque>
#include <map>
#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/dispatch_refusal.hpp>
#include <zen/weave/lifecycle.hpp>

namespace {
namespace pane = zengine::flow_pane;
namespace flow = zengine::flow;
namespace fh = zengine::flow_host;
namespace ws = zengine::workshop;
namespace input = zengine::input;
constexpr const char *workshop_role = "zengine.workshop";

class FlowPane final
    : public loom::WeaveBase<
          FlowPane, pane::FlowPaneState,
          loom::Accept<loom::Activated, ws::PaneCatalogRequested, ws::PaneRoom,
                       ws::PaneCanvasRoom, ws::PaneCanvasPointer,
                       ws::PaneCanvasRejected, ws::PaneKey, ws::PaneTextInput,
                       ws::PaneActionRequested, ws::ActionsJudged,
                       ws::PaneQuitRequested, pane::FlowEdit, fh::FlowAnswer,
                       fh::FlowCatalogAnswer, fh::FlowChanged,
                       loom::DispatchRefused>,
          loom::Emit<ws::PaneOffered, ws::PaneContent, ws::PaneCanvasContent,
                     ws::PaneActions, ws::PaneEscapeUnspent,
                     ws::PaneQuitAnswered, pane::FlowEdited, fh::FlowRun,
                     fh::FlowApply, fh::FlowSend, fh::FlowInspect, fh::FlowStop,
                     fh::FlowCatalog>> {
public:
  loom::Value snapshot() const override {
    pane::FlowPaneState saved;
    saved.workspace =
        flow::byte_vector(flow::workspace_bytes(model_.workspace));
    saved.path = model_.path;
    saved.dirty = model_.dirty;
    saved.state_edited = model_.state_edited;
    saved.page = static_cast<std::int64_t>(model_.page);
    if (model_.dialog) {
      saved.dialog_title = model_.dialog->title;
      saved.dialog_action = model_.dialog->action;
      saved.dialog_selected =
          static_cast<std::int64_t>(model_.dialog->selected);
      for (const auto &entry : model_.dialog->entries)
        saved.dialog_entries.push_back(
            {entry.label, entry.text.text(),
             static_cast<std::int64_t>(entry.text.caret()),
             static_cast<std::int64_t>(entry.text.anchor())});
    }
    return loom::to_value(saved);
  }
  void on(const loom::Activated &activated, loom::Mail &mail) {
    if (!activation_.accept(mail, activated))
      return;
    if (!restored_) {
      restored_ = true;
      if (!state_.workspace.empty())
        try {
          model_.workspace =
              flow::read_workspace(flow::byte_string(state_.workspace));
          model_.path = state_.path;
          model_.dirty = state_.dirty;
          model_.restore_form();
          model_.state_edited = state_.state_edited;
          if (state_.page >= 0 &&
              state_.page <= static_cast<std::int64_t>(pane::Page::Events))
            model_.page = static_cast<pane::Page>(state_.page);
          if (!state_.dialog_entries.empty()) {
            pane::Dialog dialog;
            dialog.title = state_.dialog_title;
            dialog.action = state_.dialog_action;
            for (const auto &entry : state_.dialog_entries) {
              pane::Entry restored;
              restored.label = entry.label;
              const auto anchor =
                  std::clamp(entry.anchor, std::int64_t{0},
                             static_cast<std::int64_t>(entry.text.size()));
              const auto caret =
                  std::clamp(entry.caret, std::int64_t{0},
                             static_cast<std::int64_t>(entry.text.size()));
              restored.text.set(entry.text, static_cast<std::size_t>(anchor));
              restored.text.drag_to_column(caret);
              dialog.entries.push_back(std::move(restored));
            }
            dialog.selected = static_cast<std::size_t>(std::clamp(
                state_.dialog_selected, std::int64_t{0},
                static_cast<std::int64_t>(dialog.entries.size() - 1)));
            model_.dialog = std::move(dialog);
          }
        } catch (const std::exception &e) {
          model_.notice = std::string("Could not restore Flow: ") + e.what();
        }
    }
    offer(mail);
    request(fh::FlowCatalog{}, "catalog", mail);
    request(fh::FlowInspect{"workshop", 0}, "inspect", mail);
  }
  void on(const ws::PaneCatalogRequested &, loom::Mail &mail) {
    if (mail.authored_from_role(workshop_role))
      offer(mail);
  }
  void on(const ws::PaneRoom &room, loom::Mail &mail) {
    if (!host(mail, room.pane))
      return;
    rows_ = room.rows;
    columns_ = room.columns;
    show(mail);
  }
  void on(const ws::PaneCanvasRoom &room, loom::Mail &mail) {
    if (!host(mail, room.pane))
      return;
    room_ = room;
    pictures_.clear();
    drag_.reset();
    picture_number_ = 0;
    show(mail);
  }
  void on(const ws::PaneCanvasRejected &answer, loom::Mail &mail) {
    if (!host(mail, answer.pane) || answer.grant != room_.grant)
      return;
    model_.notice = "Drawing rejected: " + answer.reason;
  }
  void on(const ws::ActionsJudged &answer, loom::Mail &mail) {
    if (!host(mail, answer.pane))
      return;
    if (!answer.accepted) {
      model_.notice = "Flow actions: " + answer.refusal;
      show(mail);
    }
  }
  void on(const ws::PaneActionRequested &action, loom::Mail &mail) {
    if (host(mail, action.pane))
      perform(action.id, {}, mail);
  }
  void on(const ws::PaneQuitRequested &, loom::Mail &mail) {
    if (!mail.authored_from_role(workshop_role))
      return;
    const std::string why =
        (!pending_.empty() || observed_pending_ > 0)
            ? "Flow is waiting for its host to settle work. Quit again after the result arrives."
        : model_.dialog ? "Flow has an unfinished dialog. Confirm or cancel it "
                         "before quitting."
        : model_.dirty ? "Flow has unsaved work. Save it or "
                         "explicitly Discard before quitting."
                       : "";
    (void)mail.answer(ws::PaneQuitAnswered{pane::kPane, why.empty(), why});
  }
  void on(const ws::PaneKey &key, loom::Mail &mail) {
    if (!host(mail, key.pane))
      return;
    if (model_.dialog) {
      if (key.scancode == input::scan::kEscape) {
        model_.dialog.reset(); pictures_.clear(); drag_.reset();
      } else if (key.scancode == input::scan::kReturn) {
        perform("dialog-confirm", {}, mail);
        return;
      } else if (key.scancode == input::scan::kTab) {
        auto &d = *model_.dialog;
        d.selected = (d.selected + 1) % d.entries.size();
      } else
        model_.dialog->entries.at(model_.dialog->selected)
            .text.consume(key.scancode, key.modifiers, clipboard_);
      show(mail);
      return;
    }
    if (key.scancode == input::scan::kEscape) {
      if (model_.connecting)
        model_.connecting.reset();
      else if (model_.node)
        model_.node.reset();
      else
        (void)mail.as_role(pane::kRole)
            .send_to_role(workshop_role, ws::PaneEscapeUnspent{pane::kPane},
                          mail.correlation());
      show(mail);
    } else if (key.scancode == input::scan::kDelete && model_.node)
      perform("remove", {std::to_string(*model_.node)}, mail);
  }
  void on(const ws::PaneTextInput &text, loom::Mail &mail) {
    if (!host(mail, text.pane) || !model_.dialog)
      return;
    auto &box = model_.dialog->entries.at(model_.dialog->selected).text;
    if (box.size() + text.text.size() > 4096) {
      model_.notice = "Field editing is limited to 4096 bytes";
      show(mail);
      return;
    }
    box.type(text.text);
    show(mail);
  }
  void on(const ws::PaneCanvasPointer &event, loom::Mail &mail) {
    if (!host(mail, event.pane) || event.grant != room_.grant)
      return;
    try {
      if (event.phase == ws::canvas_pointer::kLost ||
          event.phase == ws::canvas_pointer::kRelease) {
        if (drag_ && drag_->gesture == event.gesture)
          drag_.reset();
        return;
      }
      if (event.phase == ws::canvas_pointer::kMove) {
        if (!drag_ || drag_->gesture != event.gesture ||
            drag_->revision !=
                model_.workspace.graph.project.definition.revision)
          return;
        const auto ex = std::clamp(event.x, std::int64_t{-100000000},
                                   std::int64_t{100000000});
        const auto ey = std::clamp(event.y, std::int64_t{-100000000},
                                   std::int64_t{100000000});
        if (drag_->node_id == 0) {
          model_.workspace.pan_x =
              std::clamp(drag_->base_x + ex - drag_->x, std::int64_t{-10000000},
                         std::int64_t{10000000});
          model_.workspace.pan_y =
              std::clamp(drag_->base_y + ey - drag_->y, std::int64_t{-10000000},
                         std::int64_t{10000000});
        } else {
          for (auto &p : model_.workspace.graph.places)
            if (p.id == drag_->node_id) {
              p.x = std::clamp(drag_->base_x + (ex - drag_->x) * 100 /
                                                   model_.workspace.zoom,
                               std::int64_t{-10000000}, std::int64_t{10000000});
              p.y = std::clamp(drag_->base_y + (ey - drag_->y) * 100 /
                                                   model_.workspace.zoom,
                               std::int64_t{-10000000}, std::int64_t{10000000});
            }
        }
        model_.touched();
        show(mail);
        return;
      }
      if (event.phase == ws::canvas_pointer::kWheel) {
        const auto pictured = std::find_if(
            pictures_.begin(), pictures_.end(),
            [&](const auto &p) { return p.content.picture == event.picture; });
        if (pictured == pictures_.end() ||
            pictured->revision !=
                model_.workspace.graph.project.definition.revision)
          return;
        if (!std::isfinite(event.dy) || !std::isfinite(event.dx))
          return;
        const auto dy =
            static_cast<std::int64_t>(std::clamp(event.dy, -100.0, 100.0));
        if (model_.page == pane::Page::Graph && event.x > 22 * pane::unit) {
          model_.workspace.pan_y =
              std::clamp(model_.workspace.pan_y + dy * 2 * pane::unit,
                         std::int64_t{-10000000}, std::int64_t{10000000});
          model_.touched();
        } else {
          const auto rows = static_cast<std::int64_t>(model_.first_row) - dy;
          model_.first_row = static_cast<std::size_t>(
              std::clamp(rows, std::int64_t{0}, std::int64_t{4096}));
        }
        show(mail);
        return;
      }
      if (event.phase != ws::canvas_pointer::kPress)
        return;
      const auto it =
          std::find_if(pictures_.begin(), pictures_.end(), [&](const auto &p) {
            return p.content.picture == event.picture;
          });
      if (it == pictures_.end() ||
          it->revision != model_.workspace.graph.project.definition.revision) {
        model_.notice = "That picture has changed; choose again.";
        show(mail);
        return;
      }
      const auto *hit = it->hit(event.x, event.y);
      if (event.button == 2 || (!hit && event.button == 1)) {
        if (model_.page == pane::Page::Graph && !model_.dialog)
          drag_ = Drag{event.gesture,
                       0,
                       it->revision,
                       event.x,
                       event.y,
                       model_.workspace.pan_x,
                       model_.workspace.pan_y};
        return;
      }
      if (event.button != 1 || !hit)
        return;
      const auto chosen = *hit;
      if (chosen.action == "node") {
        model_.node = flow::index_of(chosen.args.at(0));
        const auto &pos =
            model_.workspace.graph.place(model_.trigger(), *model_.node);
        drag_ = Drag{event.gesture, pos.id, it->revision, event.x,
                     event.y,       pos.x,  pos.y};
        show(mail);
        return;
      }
      perform(chosen.action, chosen.args, mail);
    } catch (const std::exception &e) {
      model_.notice = e.what();
      show(mail);
    }
  }
  void on(const pane::FlowEdit &edit, loom::Mail &mail) {
    bool ok = true;
    try {
      if (model_.dialog)
        throw std::invalid_argument("Confirm or cancel the open dialog before another edit");
      pictures_.clear();
      drag_.reset();
      effect(model_.command(edit.action, edit.arguments), mail);
    } catch (const std::exception &e) {
      ok = false;
      model_.notice = e.what();
    }
    pane::FlowEdited result;
    result.ok = ok;
    result.reason = model_.notice;
    try {
      result.workspace =
          flow::byte_vector(flow::workspace_bytes(model_.workspace));
      result.problems = model_.workspace.graph.problems(model_.palette);
    } catch (const std::exception &e) {
      result.ok = false;
      result.reason = e.what();
    }
    (void)mail.answer(result);
    show(mail);
  }
  void on(const fh::FlowCatalogAnswer &answer, loom::Mail &mail) {
    if (!settle(mail, "catalog"))
      return;
    try {
      if (!answer.ok)
        throw std::invalid_argument(answer.reason);
      flow::Palette palette;
      for (const auto &record : answer.operators) {
        const auto admitted =
            loom::admit(loom::parse(flow::byte_string(record.descriptor)),
                        zengine::op::operator_desc_schema());
        if (!admitted)
          throw std::invalid_argument(admitted.first_error().message());
        const auto decoded = zengine::op::decode_contribution(admitted.value());
        if (decoded.identity != record.identity)
          throw std::invalid_argument("operator descriptor identity disagrees");
        palette.push_back({decoded.identity, decoded.inputs, decoded.outputs});
      }
      model_.palette = std::move(palette);
      model_.notice = "Host operators refreshed";
    } catch (const std::exception &e) {
      model_.notice = e.what();
    }
    show(mail);
  }
  void on(const fh::FlowAnswer &answer, loom::Mail &mail) {
    const auto pending = settle(mail, answer.action);
    if (!pending || answer.session != "workshop")
      return;
    try {
      if (!answer.ok) {
        if (!(answer.action == "inspect" && !model_.running))
          model_.notice = answer.action + " refused: " + answer.reason;
      } else {
        model_.running = answer.action != "stop";
        if (answer.action == "run" &&
            pending->state_epoch == model_.state_epoch)
          model_.state_edited = false;
        if (!answer.project.empty()) {
          auto observed = flow::read_project(flow::byte_string(answer.project));
          if (!model_.state_edited &&
              loom::same_identity(
                  *observed.definition.state,
                  *model_.workspace.graph.project.definition.state)) {
            if (loom::serialize(model_.workspace.graph.project.state) !=
                loom::serialize(observed.state)) {
              auto candidate = model_.workspace;
              candidate.graph.project.state = observed.state;
              (void)flow::workspace_bytes(candidate);
              model_.workspace = std::move(candidate);
              model_.touched();
            }
          }
          live_state_ = zengine::message_draft::Draft(observed.state);
        }
        model_.events.clear();
        model_.events.push_back(
            "Dispatch pending: " + std::to_string(answer.pending) +
            " / older events omitted: " + std::to_string(answer.dropped));
        if (live_state_)
          for (const auto &row : live_state_->rows())
            model_.events.push_back("state." + row.label + " = " + row.summary);
        for (const auto &event : answer.events) {
          model_.events.push_back("#" + std::to_string(event.sequence) + " " +
                                  event.kind + " (send " + event.correlation +
                                  ") " + event.detail);
          if (!event.payload.empty()) {
            const auto unverified =
                loom::parse(flow::byte_string(event.payload));
            // The host carries the observed shape in its project. Decode only
            // an agreed output; arbitrary event bytes remain a stated
            // observation.
            const auto &emits = model_.workspace.graph.project.definition.emits;
            for (const auto &schema : emits) {
              const auto value = loom::admit(unverified, schema);
              if (value) {
                for (const auto &row :
                     zengine::message_draft::Draft(value.value()).rows())
                  model_.events.push_back("  " + row.label + " = " +
                                          row.summary);
                break;
              }
            }
          }
        }
        // A Send answer describes the enqueue, not its result. Keep the quit
        // gate closed through the later inspection that has retained fresh
        // state. Saving the old snapshot cannot erase this outstanding work.
        observed_pending_ = answer.pending;
        model_.notice = answer.action + ": " +
                        (answer.reason.empty() ? "observed" : answer.reason);
      }
    } catch (const std::exception &e) {
      model_.notice = e.what();
    }
    show(mail);
  }
  void on(const fh::FlowChanged &changed, loom::Mail &mail) {
    if (!mail.authored_from_role(fh::kFlowHostRole) ||
        changed.session != "workshop")
      return;
    for (const auto &[correlation, p] : pending_) {
      (void)correlation;
      if (p.action == "inspect")
        return;
    }
    request(fh::FlowInspect{"workshop", 0}, "inspect", mail);
  }
  void on(const loom::DispatchRefused &refused, loom::Mail &mail) {
    if (!mail.dispatch_refused())
      return;
    for (auto it = pending_.begin(); it != pending_.end(); ++it)
      if (it->second.attempt.seq == refused.refused_attempt().seq) {
        model_.notice =
            it->second.action + " was not delivered: " + refused.reason;
        pending_.erase(it);
        show(mail);
        return;
      }
  }

private:
  struct Pending {
    std::string action;
    loom::Ticket attempt;
    std::uint64_t state_epoch = 0;
  };
  struct Drag {
    std::int64_t gesture = 0, node_id = 0, revision = 0, x = 0, y = 0,
                 base_x = 0, base_y = 0;
  };
  bool host(const loom::Mail &mail, const std::string &which) const {
    return which == pane::kPane && mail.authored_from_role(workshop_role);
  }
  std::optional<Pending> settle(loom::Mail &mail, const std::string &action) {
    if (!mail.answers_ask())
      return {};
    const auto it = pending_.find(mail.correlation());
    if (it == pending_.end() || it->second.action != action)
      return {};
    auto result = it->second;
    pending_.erase(it);
    return result;
  }
  template <class T>
  void request(const T &value, const std::string &action, loom::Mail &mail) {
    if (pending_.size() >= 16)
      throw std::invalid_argument(
          "Flow has 16 unanswered requests; inspect the host before retrying");
    const auto correlation = ++correlation_;
    const auto ticket =
        mail.as_role(pane::kRole)
            .send_to_role(fh::kFlowHostRole, value, correlation);
    if (!ticket.valid())
      throw std::invalid_argument(action + " could not be queued");
    pending_.emplace(correlation, Pending{action, ticket, model_.state_epoch});
    model_.notice = action + " queued";
  }
  void effect(const pane::Action &action, loom::Mail &mail) {
    switch (action.effect) {
    case pane::Effect::None:
      return;
    case pane::Effect::Run:
      request(fh::FlowRun{"workshop", action.payload}, "run", mail);
      break;
    case pane::Effect::Apply:
      request(fh::FlowApply{"workshop", action.payload}, "apply", mail);
      break;
    case pane::Effect::Send:
      request(fh::FlowSend{"workshop", action.payload}, "send", mail);
      break;
    case pane::Effect::Stop:
      request(fh::FlowStop{"workshop"}, "stop", mail);
      break;
    case pane::Effect::Catalog:
      request(fh::FlowCatalog{}, "catalog", mail);
      break;
    case pane::Effect::Inspect:
      request(fh::FlowInspect{"workshop", 0}, "inspect", mail);
      break;
    }
  }
  void offer(loom::Mail &mail) {

    (void)mail.as_role(pane::kRole)
        .send_to_role(
            workshop_role,
            ws::PaneOffered{pane::kPane, "Flow",
                            "author and exercise message-driven graphs"});
    (void)mail.as_role(pane::kRole)
        .send_to_role(
            workshop_role,
            ws::PaneActions{
                pane::kPane,
                {{"ask-save", "Save Flow workspace", input::scan::kS,
                  input::mod::kCtrl},
                 {"ask-open", "Open Flow workspace", input::scan::kO,
                  input::mod::kCtrl},
                 {"run", "Run Flow", input::scan::kR, input::mod::kCtrl},
                 {"ask-discard", "Discard unsaved marker", 0, 0}}});
  }
  void perform(const std::string &action, const std::vector<std::string> &args,
               loom::Mail &mail) {
    try {
      act(action, args, mail);
    } catch (const std::exception &e) {
      model_.notice = e.what();
    }
    show(mail);
  }
  void act(const std::string &action, const std::vector<std::string> &args,
           loom::Mail &mail) {
    // A control changes interaction context independently of executable
    // revision. Held gestures and old dialog/form hit maps cannot cross that
    // boundary.
    if (model_.dialog && action != "dialog-field" && action != "dialog-confirm" &&
        action != "dialog-cancel" && action != "discard")
      throw std::invalid_argument("Confirm or cancel the open dialog before another edit");
    pictures_.clear();
    drag_.reset();
    if (action == "ask-new")
      model_.ask(
          "New project (replaces the current draft)", "new",
          {{"Project name", "my_flow"}, {"Unsaved disposition", "discard"}});
    else if (action == "ask-open")
      model_.ask("Open workspace (replaces the current draft)", "open",
                 {{"Path", model_.path}, {"Unsaved disposition", "discard"}});
    else if (action == "ask-save")
      model_.ask("Save complete workspace", "save",
                 {{"Path", model_.path.empty() ? "my-flow.flow-workspace"
                                               : model_.path}});
    else if (action == "ask-export")
      model_.ask("Export executable project for the standalone Flow workbench",
                 "export-project", {{"Path", "my-flow.flow"}});
    else if (action == "ask-import")
      model_.ask(
          "Import executable Flow project (replaces this draft)",
          "import-project",
          {{"Path", "my-flow.flow"}, {"Unsaved disposition", "discard"}});
    else if (action == "ask-discard")
      model_.ask("Allow closing without saving (type discard)", "discard",
                 {{"Confirmation", ""}});
    else if (action == "discard") {
      if (args.size() != 1 || args[0] != "discard")
        throw std::invalid_argument("type discard to acknowledge unsaved work");
      model_.dirty = false;
      model_.notice = "Unsaved work may now be discarded on close";
    } else if (action == "ask-state-field")
      model_.ask(
          "State field: Int, Bool, Float, Text, Bytes, List:Kind, "
          "Message:preset",
          "state-field",
          {{"Name", "value"}, {"Kind", "Int"}, {"Presence", "required"}});
    else if (action == "ask-message")
      model_.ask("Declare an input message", "message",
                 {{"Message name", "Set"}});
    else if (action == "ask-message-field")
      model_.ask("Add field to the selected message", "message-field",
                 {{"Message index", std::to_string(model_.message)},
                  {"Name", "input"},
                  {"Kind", "Int"},
                  {"Presence", "required"}});
    else if (action == "ask-trigger")
      model_.ask("An input message updates one state field", "trigger",
                 {{"Message index", std::to_string(model_.message)},
                  {"State output field", "value"}});
    else if (action == "ask-preset")
      model_.ask("Save this value, complete or unfinished", "preset",
                 {{"Example name", model_.form_title}});
    else if (action == "ask-library-open" || action == "ask-library-save")
      model_.ask("Reusable value library",
                 action == "ask-library-open" ? "library-open" : "library-save",
                 {{"Path", "examples.flow-values"}});
    else if (action == "dialog-field") {
      model_.dialog->selected = flow::index_of(args.at(0));
    } else if (action == "dialog-cancel")
      model_.dialog.reset();
    else if (action == "dialog-confirm") {
      if (!model_.dialog)
        return;
      const auto saved = *model_.dialog;
      std::vector<std::string> values;
      for (const auto &entry : saved.entries)
        values.push_back(entry.text.text());
      // Refusal keeps the form and its authored text available for repair.
      if (saved.action == "discard")
        act("discard", values, mail);
      else
        effect(model_.command(saved.action, values), mail);
      model_.dialog.reset();
      drag_.reset();
    } else if (action == "page-graph") {
      model_.page = pane::Page::Graph;
      model_.first_row = 0;
    } else if (action == "page-state")
      effect(model_.command("state-open"), mail);
    else if (action == "page-messages") {
      model_.page = pane::Page::Messages;
      model_.first_row = 0;
    } else if (action == "page-library") {
      model_.page = pane::Page::Library;
      model_.first_row = 0;
    } else if (action == "page-events") {
      model_.page = pane::Page::Events;
      model_.first_row = 0;
    } else if (action == "source-field") {
      model_.connecting = zengine::op::Binding::input(args.at(0));
      model_.notice = "Choose an input port to connect " + args.at(0);
    } else if (action == "source-node") {
      model_.connecting =
          zengine::op::Binding::node(flow::index_of(args.at(0)));
      model_.notice = "Choose an input port to connect node " + args.at(0);
    } else if (action == "port") {
      if (model_.connecting) {
        const auto binding = *model_.connecting;
        const auto text = binding.from() == zengine::op::Binding::From::Input
                              ? "$" + binding.input_name()
                              : "%" + std::to_string(binding.node_index());
        effect(model_.command("bind", {args.at(0), args.at(1), text}), mail);
        model_.connecting.reset();
      } else {
        model_.ask(
            "Bind a constant, $field, or %earlier-node", "bind",
            {{"Node", args.at(0)}, {"Port", args.at(1)}, {"Value", "0"}});
        model_.dialog->selected = 2;
      }
    } else if (action == "value-row") {
      const auto row = model_.form->rows().at(flow::index_of(args.at(0)));
      if (row.type.kind == loom::Kind::Message ||
          row.type.kind == loom::Kind::List)
        effect(model_.command("container", args), mail);
      else {
        model_.ask("Edit " + row.label, "value",
                   {{"Row", args.at(0)},
                    {"Value", row.present && model_.form->get(row.path)
                                  ? raw(*model_.form->get(row.path))
                                  : ""}});
        model_.dialog->selected = 1;
      }
    } else if (action == "fit") {
      model_.workspace.pan_x = 0;
      model_.workspace.pan_y = -3 * pane::unit;
      model_.workspace.zoom = 75;
      model_.touched();
    } else if (action == "zoom-in" || action == "zoom-out") {
      model_.workspace.zoom =
          std::clamp(model_.workspace.zoom + (action == "zoom-in" ? 25 : -25),
                     std::int64_t{50}, std::int64_t{200});
      model_.touched();
    } else {
      effect(model_.command(action, args), mail);
      drag_.reset();
    }
  }
  static std::string raw(const loom::Cell &cell) {
    if (cell.kind() == loom::Kind::Text)
      return cell.as_text();
    if (cell.kind() == loom::Kind::Int)
      return std::to_string(cell.as_int());
    if (cell.kind() == loom::Kind::Bool)
      return cell.as_bool() ? "true" : "false";
    if (cell.kind() == loom::Kind::Float)
      return std::to_string(cell.as_float());
    return zengine::message_draft::summary(&cell);
  }
  void show(loom::Mail &mail) {
    if (room_.grant > 0 && room_.width > 0 && room_.height > 0) {
      auto current = pane::picture(model_, room_, ++picture_number_);
      const auto ticket = mail.as_role(pane::kRole)
                              .send_to_role(workshop_role, current.content);
      if (ticket.valid()) {
        pictures_.push_back(std::move(current));
        while (pictures_.size() > 8)
          pictures_.pop_front();
      }
    } else if (rows_ > 0 && columns_ > 0) {
      std::vector<zengine::surface::SurfaceTextRow> rows;
      auto lines = flow::graph_lines(model_.workspace.graph.project.definition);
      lines.insert(lines.begin(),
                   "Flow: graphical editing needs a canvas-capable Workshop. "
                   "Use FlowEdit or the standalone workbench.");
      lines.push_back(model_.notice);
      for (const auto &line : lines) {
        if (static_cast<std::int64_t>(rows.size()) >= rows_)
          break;
        rows.push_back({ws::pane_text::fit(pane::clean(line), columns_)});
      }
      (void)mail.as_role(pane::kRole)
          .send_to_role(workshop_role,
                        ws::PaneContent{pane::kPane, std::move(rows)});
    }
  }
  pane::Model model_;
  zengine::ActivationCursor activation_;
  bool restored_ = false;
  ws::PaneCanvasRoom room_;
  std::int64_t rows_ = 0, columns_ = 0, picture_number_ = 0;
  std::uint64_t correlation_ = 0;
  std::map<std::uint64_t, Pending> pending_;
  std::int64_t observed_pending_ = 0;
  std::optional<zengine::message_draft::Draft> live_state_;
  std::deque<pane::Picture> pictures_;
  std::optional<Drag> drag_;
  zengine::component::Clipboard clipboard_;
};
} // namespace
ZEN_EXPORT_WEAVE(FlowPane)
