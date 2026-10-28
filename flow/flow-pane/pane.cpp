// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "flow/flow-host/vocabulary.hpp"
#include "flow/flow-pane/view.hpp"
#include "flow/flow-pane/vocabulary.hpp"
#include "graphics/canvas.hpp"
#include "graphics/vocabulary.hpp"
#include "inventory/codec.hpp"
#include "operator/reference.hpp"
#include "workshop/actor_scope.hpp"
#include "workshop/fingerprint.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_drawing.hpp"
#include "workshop/pane_facets.hpp"
#include "input/vocabulary.hpp"
#include "operator/host.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/powers_vocabulary.hpp"
#include "manuals/zengine.flow.hpp" // this pane's manual, compiled in from its page
#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <string>
#include <tuple>
#include <utility>
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
namespace gfx = zengine::graphics;
constexpr const char *workshop_role = "zengine.workshop";

/// WHAT THE FLOW HOST'S YES SAYS IN THE PANE'S LINE, in the weaver's words; an inspection says
/// nothing of itself, its events and state being the pane's to show.
std::string done(const fh::FlowAnswer &answer, const std::string &said) {
  if (answer.action == "run") return "Running";
  if (answer.action == "apply") return "Applied to the running flow";
  if (answer.action == "stop") return "Stopped";
  if (answer.action == "send") return "Sent; Events shows what it did";
  if (answer.action == "inspect") return said;
  return answer.action + ": " + (answer.reason.empty() ? "observed" : answer.reason);
}

/// ONE THING FLOW ASKED AND STILL WAITS ON (`AskFacet`): a request of the Flow host, by its action;
/// the question to the discovery door; a file write a dialog confirmed, waiting on Workshop's word
/// for the confirming actor, with the verb and values as confirmed; or a declared message's shape
/// being carried out, with the gesture it continues and the bytes Workshop will carry.
struct FlowAsk {
  enum class Kind { kHost, kFind, kWrite, kCarry };
  Kind kind = Kind::kHost;
  std::string action;              ///< the host request's action, or the write's verb
  std::uint64_t state_epoch = 0;   ///< the state a host request was asked about
  std::uint64_t request = 0;       ///< which host request this is, for an Add waiting on it
  std::vector<std::string> values; ///< the write's values, as confirmed
  std::uint64_t gesture = 0;       ///< the gesture a carry continues
  std::string label;               ///< what a carry carries, by name
  loom::Bytes bytes;               ///< ...and its bytes
  bool drag = false;               ///< a release places the carry, not a click
};

inline FlowAsk asking(FlowAsk::Kind kind) {
  FlowAsk a;
  a.kind = kind;
  return a;
}

inline auto of_kind(FlowAsk::Kind kind) {
  return [kind](const FlowAsk &a) { return a.kind == kind; };
}

using Room = ws::PaneFacet<pane::kPane, pane::kRole, zengine::manual::pages::zengine_flow::kManual,
                           ws::v3::PaneOffered>;
using Asks = ws::AskFacet<pane::kPane, pane::kRole, FlowAsk>;
using Actions = ws::PaneActionFacet<pane::kPane, pane::kRole>;
using Menu = ws::PaneMenuFacet<pane::kPane, pane::kRole>;

class FlowPane final
    : public zengine::With<Room, Asks, Actions, Menu>::Weave<
          FlowPane, pane::FlowPaneState,
          loom::Accept<ws::PaneKey, ws::PaneTextInput, ws::ActionsJudged,
                       ws::PaneQuitRequested, pane::FlowEdit, fh::FlowAnswer,
                       fh::FlowCatalogAnswer, fh::FlowChanged, ws::PowersFound,
                       ws::PaneCanvasValueDrop, ws::PaneCarryAnswered, gfx::AreaRoom,
                       gfx::DrawnRefused, gfx::ActionRequested, gfx::AreaButtonChanged,
                       gfx::AreaPointerMoved, gfx::AreaWheelTurned, gfx::AreaHoldEnded>,
          loom::Emit<ws::PaneEscapeUnspent,
                     ws::PanePassRequested, ws::PaneQuitAnswered, pane::FlowEdited, fh::FlowRun,
                     fh::FlowApply, fh::FlowSend, fh::FlowInspect, fh::FlowStop, fh::FlowCatalog,
                     ws::PaneOperationRequested, ws::PaneValueCarryRequested, gfx::Drawn,
                     gfx::RoomAsked>> {
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
    saved.started = model_.started ? pane::kept_start(*model_.started)
                                   : model_.started_kept;
    saved.earlier = model_.earlier;
    return loom::to_value(saved);
  }
  void on_activated(loom::Mail &mail) {
    // AN ACTIVATION ENDS A WRITE STILL ASKED: a successor's ask numbers start again, so an
    // earlier answer could be read as a new ask's. Its dialog stays, to be confirmed again.
    if (const FlowAsk *ended = asks().find(of_kind(FlowAsk::Kind::kWrite))) {
      model_.notice = pane::Model::writing(ended->action) + " was not done: confirm it again";
      (void)asks().forget(of_kind(FlowAsk::Kind::kWrite));
    }
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
          model_.started_kept = state_.started;
          model_.earlier = state_.earlier;
          if (model_.earlier.size() > pane::Model::kMaxEarlier)
            model_.earlier.erase(model_.earlier.begin(),
                                 model_.earlier.end() -
                                     static_cast<std::ptrdiff_t>(pane::Model::kMaxEarlier));
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
    room().announce(*this, mail);
    request(fh::FlowCatalog{}, "catalog", mail);
    request(fh::FlowInspect{"workshop", 0}, "inspect", mail);
    find(mail, true);
    // ...and its area's room, once a life: a pane started again draws in its window again.
    if (!room_asked_ && !window_.shown()) {
      room_asked_ = true;
      (void)mail.as_role(pane::kRole).send_to_role(gfx::kGraphicsRole,
                                                   gfx::RoomAsked{window_.area()});
    }
  }
  ws::v3::PaneOffered offering() const {
    return ws::v3::PaneOffered{pane::kPane, "Flow", "author and exercise message-driven graphs",
                               704, 396, 0};
  }
  void offered(loom::Mail &mail) { declare(mail, true); }
  void on(const ws::ActionsJudged &answer, loom::Mail &mail) {
    if (!host(mail, answer.pane))
      return;
    if (!answer.accepted) {
      model_.notice = "Flow actions: " + answer.refusal;
      show(mail);
    }
  }
  void on_action(const std::string &id, loom::Mail &mail) { perform(id, {}, mail); }
  void on(const ws::PaneQuitRequested &, loom::Mail &mail) {
    if (!mail.authored_from_role(workshop_role))
      return;
    const std::string why =
        (asks().holds(of_kind(FlowAsk::Kind::kHost)) || observed_pending_ > 0)
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
        model_.dialog.reset(); drag_.reset();
        drop_writing();
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
    if (model_.wire_search) {
      // A WIRE'S SEARCH HOLDS THE KEYS: Escape puts it and the wire down, the arrows move among
      // its rows, Return makes the row the keys are on, and the rest edit the search line.
      if (key.scancode == input::scan::kEscape) {
        put_wire_down("Escape");
      } else if (key.scancode == input::scan::kUp || key.scancode == input::scan::kDown) {
        step_wire_choice(key.scancode == input::scan::kDown);
      } else if (key.scancode == input::scan::kReturn) {
        const auto rows = pane::wire_rows(model_);
        if (const auto choice = pane::wire_choice(model_, rows)) {
          perform("wire-row", {std::to_string(*choice)}, mail);
          return;
        }
      } else if (!model_.search.consume(key.scancode, key.modifiers, clipboard_)) {
        return;
      }
      show(mail);
      return;
    }
    if (key.scancode == input::scan::kEscape) {
      // One layer per press, the most specific first; the search line clears before the
      // pane is put down.
      if (model_.dropped)
        model_.dropped.reset();
      else if (model_.connecting)
        model_.connecting.reset();
      else if (model_.filling)
        model_.filling.reset();
      else if (model_.body_slot) {
        model_.body_slot.reset();
        model_.slot_reference.reset();
      }
      else if (!model_.preview.empty()) {
        model_.preview.clear();
        model_.preview_kind.clear();
      }
      else if (model_.node)
        model_.node.reset();
      else if (!model_.search.empty())
        model_.search.clear();
      else
        (void)mail.as_role(pane::kRole)
            .send_to_role(workshop_role, ws::PaneEscapeUnspent{pane::kPane},
                          mail.correlation());
      show(mail);
    } else if (key.scancode == input::scan::kDelete && model_.node) {
      perform("remove", {std::to_string(*model_.node)}, mail);
    } else if (key.scancode == input::scan::kReturn) {
      if (const auto *row = pane::previewed(model_))
        perform("add-found", {row->kind, row->identity}, mail);
    } else if (model_.search.consume(key.scancode, key.modifiers, clipboard_)) {
      show(mail);
    }
  }
  void on(const ws::PaneTextInput &text, loom::Mail &mail) {
    if (!host(mail, text.pane))
      return;
    // Outside a dialog, typed text is the search line's: it is the one field there.
    auto &box = model_.dialog ? model_.dialog->entries.at(model_.dialog->selected).text
                              : model_.search;
    if (box.size() + text.text.size() > 4096) {
      model_.notice = "Field editing is limited to 4096 bytes";
      show(mail);
      return;
    }
    box.type(text.text);
    show(mail);
  }
  /// THE POINTER IN THE WINDOW, given by the window that shows Flow's area as its office, read
  /// against the picture last drawn there and the room it was drawn in, a press's moves and release
  /// under the window's number for it, a hold the window ended a loss of the drag.
  void on(const gfx::AreaButtonChanged &b, loom::Mail &mail) { on_window(window_.pointer(b, mail, 0, 0), mail); }
  void on(const gfx::AreaPointerMoved &m, loom::Mail &mail) { on_window(window_.pointer(m, mail, 0, 0), mail); }
  void on(const gfx::AreaWheelTurned &w, loom::Mail &mail) { on_window(window_.pointer(w, mail, 0, 0), mail); }
  void on(const gfx::AreaHoldEnded &e, loom::Mail &mail) { on_window(window_.pointer(e, mail), mail); }
  void on_window(const std::optional<ws::WindowPointer> &p, loom::Mail &mail) {
    if (!p)
      return;
    const std::int64_t gesture = p->phase == ws::canvas_pointer::kLost
                                     ? (drag_ ? drag_->gesture : 0)
                                     : static_cast<std::int64_t>(p->press);
    // EVERY DIALOG NAMES ITS PARTS ALIKE, so one landed under another dialog's number is another.
    const bool other_dialog = p->told && p->drawing != dialog_number() &&
                              (p->part.rfind("dialog-", 0) == 0 ||
                               p->part.rfind("control:dialog-", 0) == 0);
    const Moment event{gesture, p->phase, p->button, p->x, p->y, p->dx, p->dy,
                       p->moved || other_dialog};
    pointer(event, window_shown_ ? &*window_shown_ : nullptr, window_room_, mail);
  }
  /// ONE MOMENT OF THE POINTER IN FLOW'S AREA: its press's number, its phase
  /// (`ws::canvas_pointer`), its button, its place in the area's pixels, the wheel's turn, and
  /// whether the window drew another part there than Flow's picture shows now.
  struct Moment {
    std::int64_t gesture = 0, phase = 0, button = 0, x = 0, y = 0;
    double dx = 0, dy = 0;
    bool moved = false;
  };
  /// ...about `pictured`, the picture last drawn there (none before one is), in `room`.
  void pointer(const Moment &event, const pane::Picture *pictured,
               const ws::CanvasRoom &room, loom::Mail &mail) {
    try {
      if (event.phase == ws::canvas_pointer::kLost ||
          event.phase == ws::canvas_pointer::kRelease) {
        if (drag_ && drag_->gesture == event.gesture) {
          const Drag held = *drag_;
          drag_.reset();
          if (held.wire)
            let_go(held, event, mail);
        }
        return;
      }
      if (event.phase == ws::canvas_pointer::kMove) {
        if (!drag_ || drag_->gesture != event.gesture ||
            drag_->revision !=
                model_.workspace.graph.project.definition.revision)
          return;
        if (drag_->wire) {
          wire_moved(event, room, mail);
          return;
        }
        const auto ex = std::clamp(event.x, std::int64_t{-100000000},
                                   std::int64_t{100000000});
        const auto ey = std::clamp(event.y, std::int64_t{-100000000},
                                   std::int64_t{100000000});
        const pane::GridProjection grid(room);
        const auto dx = grid.grid_x(ex - drag_->x), dy = grid.grid_y(ey - drag_->y);
        if (drag_->node_id == 0) {
          model_.workspace.pan_x =
              std::clamp(drag_->base_x + dx, std::int64_t{-10000000},
                         std::int64_t{10000000});
          model_.workspace.pan_y =
              std::clamp(drag_->base_y + dy, std::int64_t{-10000000},
                         std::int64_t{10000000});
        } else {
          for (auto &p : model_.workspace.graph.places)
            if (p.id == drag_->node_id) {
              p.x = std::clamp(drag_->base_x + dx * 100 / model_.workspace.zoom,
                               std::int64_t{-10000000}, std::int64_t{10000000});
              p.y = std::clamp(drag_->base_y + dy * 100 / model_.workspace.zoom,
                               std::int64_t{-10000000}, std::int64_t{10000000});
            }
        }
        model_.touched();
        show(mail);
        return;
      }
      if (event.phase == ws::canvas_pointer::kWheel) {
        if (pictured == nullptr ||
            pictured->revision !=
                model_.workspace.graph.project.definition.revision)
          return;
        if (!std::isfinite(event.dy) || !std::isfinite(event.dx))
          return;
        const auto dy =
            static_cast<std::int64_t>(std::clamp(event.dy, -100.0, 100.0));
        if (model_.page == pane::Page::Graph && pictured->graph && event.x >= pictured->graph->x) {
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
      // A PRESS THE WINDOW LANDED ON ANOTHER PART than Flow's picture shows there now acts on
      // nothing, and is said.
      if (event.moved && (event.button == 1 || event.button == 3)) {
        model_.notice = "The picture changed under the press; press again";
        show(mail);
        return;
      }
      // WITH A WIRE'S SEARCH OPEN a press on one of its rows makes it, a press elsewhere in the
      // list does nothing, and a press anywhere else closes it and is spent on nothing more.
      if (model_.wire_search) {
        const auto *on = pictured != nullptr &&
                                 pictured->revision ==
                                     model_.workspace.graph.project.definition.revision
                             ? pictured->hit(event.x, event.y)
                             : nullptr;
        if (event.button == 1 && on != nullptr && on->action == "wire-row") {
          const auto row = *on; // a copy: performing it clears the pictures it points into
          perform(row.action, row.args, mail);
          return;
        }
        if (event.button != 1 || on == nullptr || on->action != "wire-list")
          put_wire_down("a press outside its list");
        show(mail);
        return;
      }
      // A RIGHT PRESS MEANS NOTHING ON THE GRAPH OR ITS PAGES: handed back, so the host's pane
      // menu opens where it landed.
      if (event.button == 3) {
        // A RIGHT PRESS ON A DECLARED MESSAGE offers to carry its shape out; anywhere else it is
        // handed back, so Workshop's pane menu opens where it landed.
        if (const auto shape = pressed_message(event, pictured)) {
          menu().send(mail, menu().offer(shape->name()).row("carry", "Carry " + shape->name()));
          return;
        }
        (void)ws::pane_menu::pass_back(mail, pane::kRole, pane::kPane);
        return;
      }
      const pane::Picture *it = pictured;
      if (it == nullptr ||
          it->revision != model_.workspace.graph.project.definition.revision) {
        model_.notice = "That picture has changed; choose again.";
        show(mail);
        return;
      }
      const auto *hit = it->hit(event.x, event.y);
      if (event.button == 2 || (!hit && event.button == 1)) {
        // A PRESS ON EMPTY SPACE LETS GO of the chosen step and its port; its drag moves the view.
        if (!hit && (model_.node || model_.filling)) {
          model_.node.reset();
          model_.filling.reset();
          show(mail);
        }
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
      // A PRESS ON A DECLARED MESSAGE may drag its shape out: asked under the press, it is
      // carried only if the hand moves before it lets go, and the press still opens the row.
      if (const auto shape = pressed_message(event, pictured))
        carry(shape, mail.correlation(), true, mail);
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
      // A PRESS ON A WIRE'S END does what it does today, and the press is held: moved past a
      // character cell, it drags the wire, and its release says where the wire goes.
      const auto end = wire_end(chosen);
      perform(chosen.action, chosen.args, mail);
      if (end && model_.page == pane::Page::Graph && !model_.dialog) {
        Drag wire{event.gesture, 0, model_.workspace.graph.project.definition.revision,
                  event.x, event.y, 0, 0};
        wire.wire = true;
        wire.from = *end;
        wire.origin = chosen.action;
        wire.origin_args = chosen.args;
        drag_ = std::move(wire);
      }
    } catch (const std::exception &e) {
      model_.notice = e.what();
      show(mail);
    }
  }
  /// THE ROOM, from the graphics window that shows Flow's area, as that window's office: a window,
  /// an incarnation or a size it has not drawn in holds none of its picture, so it goes whole.
  void on(const gfx::AreaRoom &r, loom::Mail &mail) {
    if (window_.room(r, mail)) {
      window_parts_.clear();
      show_window(mail);
    }
  }
  /// A REFUSED CHANGE is drawn again whole at once, against nothing the window lacks; a refused
  /// whole picture is said in the notice and nothing goes there until the picture changes, so a
  /// window that keeps refusing starts no loop (`ws::WindowDrawing::refused`).
  void on(const gfx::DrawnRefused &refused, loom::Mail &mail) {
    if (!window_.refused(refused, mail) || !window_.refused_whole())
      return;
    model_.notice = "The graphics window refused Flow's picture: " + refused.reason;
    show(mail);
  }
  /// A PRESS ASKED AS A PART'S ACTION, by a window that shows Flow's area placed deaf to the
  /// pointer: what a first-button press on that place of Workshop's canvas does, a node selected and
  /// anything else performed. No drag begins and nothing is carried, since no hand holds a press
  /// Workshop numbered there.
  void on(const gfx::ActionRequested &asked, loom::Mail &mail) {
    if (!window_.from_window(asked.area, mail))
      return;
    const auto part = window_parts_.find(asked.action);
    if (part == window_parts_.end())
      return;
    const pane::Hit chosen = part->second;
    if (model_.wire_search && chosen.action != "wire-row" && chosen.action != "wire-list") {
      put_wire_down("a press outside its list");
      show(mail);
      return;
    }
    try {
      if (chosen.action == "node") {
        model_.node = flow::index_of(chosen.args.at(0));
        show(mail);
        return;
      }
    } catch (const std::exception &e) {
      model_.notice = e.what();
      show(mail);
      return;
    }
    perform(chosen.action, chosen.args, mail);
  }
  /// A VALUE A PERSON CARRIED HERE, placed on the picture Flow last drew in its area. An operator
  /// reference becomes a node -- into the port it landed on, as the body of the fold whose slot it
  /// landed on, or where it was released -- and a stale one is refused; any other value is held
  /// on its own page until the maker says what it becomes. Carrying it granted nothing.
  void on(const ws::PaneCanvasValueDrop &drop, loom::Mail &mail) {
    if (!host(mail, drop.pane) || drop.area != window_.area())
      return;
    try {
      if (model_.dialog)
        throw std::invalid_argument("Confirm or cancel the open dialog before dropping a value");
      const pane::Picture *it = window_shown_ ? &*window_shown_ : nullptr;
      if (it == nullptr ||
          it->revision != model_.workspace.graph.project.definition.revision ||
          window_.moved(drop.under, drop.part, drop.drawing, drop.x, drop.y))
        throw std::invalid_argument("The picture changed while the value was carried; drop it again");
      const auto value = zengine::inventory::decode_pair(
          std::string_view(reinterpret_cast<const char *>(drop.data.data()), drop.data.size())).item;
      const auto *hit = it->hit(drop.x, drop.y);
      if (zengine::op::is_reference(value)) {
        drop_reference(zengine::op::decode_reference(value), hit, *it, drop, mail);
        reveal();
      } else if (hit && hit->action == "drop-zone") {
        const auto shape = pane::dropped_shape(value);
        if (!shape)
          throw std::invalid_argument("That carries no shape to declare: " +
                                      pane::summarize_drop(value).why);
        model_.declare(shape, false);
        const auto &def = model_.workspace.graph.project.definition;
        if (def.on.empty() && !def.state->fields().empty())
          effect(model_.command("trigger", {"0", def.state->fields().front().name}), mail);
        model_.notice = "Declared " + shape->name() + "; it wakes the rule" +
                        (def.on.empty() ? " once it keeps something" : "");
      } else {
        std::optional<pane::PortChoice> port;
        if (hit && hit->action == "port" && hit->args.size() == 2)
          port = pane::PortChoice{hit->subject, flow::index_of(hit->args[1])};
        model_.dropped = pane::Dropped{value, port};
        const auto summary = pane::summarize_drop(value);
        model_.notice = summary.why.empty() ? summary.title + ": choose what it becomes here"
                                            : "Not declarable: " + summary.why;
      }
    } catch (const std::exception &e) {
      model_.notice = e.what();
    }
    drag_.reset();
    show(mail);
  }
  /// THE ROWS OF FLOW'S OWN MENUS, by the control that opens each: File, View and the gear, each
  /// row a command Flow declares.
  static const std::vector<std::pair<const char *, const char *>> &menu_rows(const std::string &which) {
    static const std::vector<std::pair<const char *, const char *>> kFile{
        {"ask-new", "New..."},       {"ask-open", "Open..."},     {"ask-save", "Save"},
        {"ask-import", "Import..."}, {"ask-export", "Export..."}, {"ask-generate", "Generate C++..."},
        {"ask-discard", "Discard"}};
    static const std::vector<std::pair<const char *, const char *>> kView{
        {"fit", "Fit"},           {"zoom-in", "Zoom in"},  {"zoom-out", "Zoom out"},
        {"find-wire", "Find a wire"}, {"inspect", "Inspect"}, {"catalog", "Catalog"}};
    static const std::vector<std::pair<const char *, const char *>> kMore{
        {"ask-trigger", "Add a trigger..."}, {"ask-keep", "Keep a field..."},
        {"ask-say", "Say a message..."},     {"ask-emit", "Add an emit..."},
        {"restart", "Restart"}};
    static const std::vector<std::pair<const char *, const char *>> kNone{};
    return which == "file" ? kFile : which == "view" ? kView : which == "more" ? kMore : kNone;
  }
  /// OPEN ONE OF FLOW'S MENUS where the press that asked for it was made.
  void open_menu(const std::string &which, loom::Mail &mail) {
    const auto &rows = menu_rows(which);
    if (rows.empty())
      throw std::invalid_argument("Flow has no menu `" + which + "`");
    auto offer = menu().offer(which);
    for (const auto &[id, label] : rows)
      offer.row(id, label);
    menu().send(mail, offer);
  }
  /// THE MENU'S CHOICE: a row of File, View or the gear does its command; a carry carries the
  /// declared message's shape out as a description, under the gesture the choice continues -- the
  /// message must still be declared, or nothing is carried.
  void on_menu(const std::string &id, const std::string &subject, loom::Mail &mail) {
    const auto &rows = menu_rows(subject);
    if (!rows.empty()) {
      if (std::any_of(rows.begin(), rows.end(), [&](const auto &row) { return id == row.first; }))
        perform(id, {}, mail);
      return;
    }
    if (id != "carry")
      return;
    const auto shape = declared(subject);
    if (!shape) {
      model_.notice = subject + " is no longer declared here; nothing was carried";
      show(mail);
      return;
    }
    carry(shape, mail.correlation(), false, mail);
  }
  /// WORKSHOP'S WORD ON A CONFIRMED WRITE, or on a carry: each is the exact ask `AskFacet` held.
  void on_approval(const FlowAsk &asked, const ws::PaneOperationAnswered &answer,
                   loom::Mail &mail) {
    if (asked.kind == FlowAsk::Kind::kWrite) {
      settle_write(asked, answer, mail);
      return;
    }
    if (asked.kind != FlowAsk::Kind::kCarry)
      return;
    if (!answer.allowed) {
      model_.notice = "Carrying " + asked.label + " was refused: " + answer.reason;
      show(mail);
      return;
    }
    (void)mail.as_role(pane::kRole).send_to_role(
        workshop_role,
        ws::PaneValueCarryRequested{pane::kPane, asked.label, asked.bytes, asked.drag},
        asked.gesture);
  }
  void on(const ws::PaneCarryAnswered &answer, loom::Mail &mail) {
    if (!answer.carried) {
      model_.notice = "Not carried: " + answer.reason;
      show(mail);
    }
  }
  void on(const pane::FlowEdit &edit, loom::Mail &mail) {
    bool ok = true;
    try {
      if (model_.dialog)
        throw std::invalid_argument("Confirm or cancel the open dialog before another edit");
      drag_.reset();
      model_.wire_search.reset();
      model_.wire_drawn.reset();
      effect(model_.command(edit.action, edit.arguments), mail);
      if (edit.action == "add-node" || edit.action == "add-node-into" ||
          edit.action == "add-fold" || edit.action == "fold-body")
        reveal();
      if ((edit.action == "open" || edit.action == "import-project"))
        pane::reveal_graph(model_, window_room_);
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
    const std::optional<FlowAsk> catalog = settle(mail, "catalog");
    if (!catalog)
      return;
    // An Add waiting on this request is settled by its answer, whatever the answer says.
    std::optional<Adding> adding;
    if (adding_ && adding_->request == catalog->request)
      adding = std::exchange(adding_, std::nullopt);
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
      if (adding) {
        if (flow::workspace_bytes(model_.workspace) != adding->draft)
          model_.notice = "The graph changed while the ports of " + adding->identity +
                          " were read; Add it again";
        else if (described(adding->identity)) {
          effect(model_.command(adding->command, adding->arguments), mail);
          reveal();
        }
        else
          model_.notice = adding->identity + " is not in the host's catalog now";
      }
    } catch (const std::exception &e) {
      model_.notice = e.what();
    }
    show(mail);
  }
  /// The discovery door's answer to this pane's question -- the latest one asked, by Loom's
  /// answer provenance and the correlation, so an answer the maker has typed past is dropped.
  /// Kept whole until the next, and never read as what the catalog holds now.
  void on(const ws::PowersFound &said, loom::Mail &mail) {
    if (!asks().answered(mail, of_kind(FlowAsk::Kind::kFind)))
      return;
    model_.discovered = said;
    model_.discovered_read = true;
    if (model_.wire_search)
      model_.wire_search->answered = true;
    show(mail);
  }
  void on(const fh::FlowAnswer &answer, loom::Mail &mail) {
    const auto pending = settle(mail, answer.action);
    if (!pending || answer.session != "workshop")
      return;
    // A RESTART RUNS THE DRAFT once its stop is answered, and only then: another answer between
    // them leaves it waiting.
    const bool restarting = answer.action == "stop" && std::exchange(restarting_, false);
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
          // WHAT RUNS NOW, the state it was run with, and what its journal says it did, each read
          // against the definition the host runs.
          // A RUN STARTS A JOURNAL AFRESH: what reached the run before it goes on in the Events.
          if (answer.action == "run") {
            model_.started = observed.state;
            model_.started_kept.clear();
            for (const auto &arrival : model_.observed.arrivals)
              model_.earlier.push_back(pane::line_of(arrival));
            if (model_.earlier.size() > pane::Model::kMaxEarlier)
              model_.earlier.erase(model_.earlier.begin(),
                                   model_.earlier.end() -
                                       static_cast<std::ptrdiff_t>(pane::Model::kMaxEarlier));
          }
          // A RESTORED PANE'S RUN is the one its first answer names: the state it was run with,
          // kept as text, is read back against that and is not held for any later run.
          if (!model_.started_kept.empty()) {
            if (!model_.started)
              model_.started = pane::start_of(model_.started_kept, observed.definition);
            model_.started_kept.clear();
          }
          if (model_.running)
            model_.running_definition = observed.definition;
          model_.observed = pane::observe(answer, observed.definition, model_.started);
        }
        if (answer.action == "run")
          model_.ran = false;
        if (answer.action == "stop") {
          model_.ran = true;
          model_.running_definition.reset();
        }
        // THE EVENTS PAGE: one line a message, newest first, then the host's other words and
        // what the journal holds.
        model_.events.clear();
        for (auto at = model_.observed.arrivals.rbegin(); at != model_.observed.arrivals.rend(); ++at)
          model_.events.push_back(pane::line_of(*at));
        for (auto at = model_.earlier.rbegin(); at != model_.earlier.rend(); ++at)
          model_.events.push_back(*at);
        for (const auto &note : model_.observed.notes)
          model_.events.push_back(note);
        if (live_state_)
          for (const auto &row : live_state_->rows())
            model_.events.push_back("state." + row.label + " = " + row.summary);
        model_.events.push_back("Dispatch pending: " + std::to_string(answer.pending) +
                                " / older events omitted: " + std::to_string(answer.dropped));
        // A Send answer describes the enqueue, not its result. Keep the quit
        // gate closed through the later inspection that has retained fresh
        // state. Saving the old snapshot cannot erase this outstanding work.
        observed_pending_ = answer.pending;
        model_.notice = done(answer, model_.notice);
        // Running, applying or stopping a definition mounts or unmounts its contributions, so
        // what the door would say may have moved: it is asked again rather than told.
        if (answer.action == "run" || answer.action == "apply" || answer.action == "stop")
          find(mail, true);
        if (restarting) {
          effect(model_.command("run"), mail);
          model_.notice = "Restarting: the draft runs, keeping what it kept";
        }
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
    if (asks().holds([](const FlowAsk &a) {
          return a.kind == FlowAsk::Kind::kHost && a.action == "inspect";
        }))
      return;
    // AN INSPECTION FLOW ASKS BY ITSELF says nothing: the line keeps what the weaver last did.
    const auto said = model_.notice;
    request(fh::FlowInspect{"workshop", 0}, "inspect", mail);
    model_.notice = said;
  }
  /// LOOM'S WORD THAT EXACTLY ONE OF FLOW'S ASKS WAS REFUSED BEFORE ANY HANDLER RAN, matched by
  /// `AskFacet`: the question to the door, a write Workshop never heard, or a host request -- and
  /// an Add waiting on that request, which will never be answered. A carry's refusal says nothing.
  void on_refused(const FlowAsk &asked, const loom::DispatchRefused &refused, loom::Mail &mail) {
    switch (asked.kind) {
    case FlowAsk::Kind::kFind:
      model_.notice = "finding powers was not delivered: " + refused.reason;
      break;
    case FlowAsk::Kind::kWrite:
      model_.notice = pane::Model::writing(asked.action) +
                      " was not done: Workshop could not be asked (" + refused.reason + ")";
      break;
    case FlowAsk::Kind::kHost:
      model_.notice = asked.action + " was not delivered: " + refused.reason;
      if (adding_ && adding_->request == asked.request)
        adding_.reset(); // the Add waited on this request, which will never be answered
      if (asked.action == "stop")
        restarting_ = false; // a stop never delivered runs nothing after it
      break;
    case FlowAsk::Kind::kCarry: return;
    }
    show(mail);
  }

private:
  struct Drag {
    std::int64_t gesture = 0, node_id = 0, revision = 0, x = 0, y = 0,
                 base_x = 0, base_y = 0;
    /// A wire's drag: the end it was pressed on, as a part and as an end, and whether the hand
    /// has moved past the threshold that makes it a drag rather than a click.
    bool wire = false, crossed = false;
    pane::WireEnd from{};
    std::string origin{};
    std::vector<std::string> origin_args{};
  };
  /// The end of a wire a press on `hit` takes hold of: an output button, a field in scope, or an
  /// input port while no wire is in hand; none for anything else.
  std::optional<pane::WireEnd> wire_end(const pane::Hit &hit) const {
    if (hit.action == "source-node" && hit.args.size() == 1)
      return pane::WireEnd{zengine::op::Binding::node(flow::index_of(hit.args[0])), {}};
    if ((hit.action == "source-field" || hit.action == "field") && hit.args.size() == 1)
      return pane::WireEnd{zengine::op::Binding::input(hit.args[0]), {}};
    if (hit.action == "port" && hit.args.size() == 2 && !model_.connecting)
      return pane::WireEnd{{}, pane::PortChoice{hit.subject, flow::index_of(hit.args[1])}};
    return std::nullopt;
  }
  /// The picture last drawn in the window, which a drag's later moments are read against.
  const pane::Picture *latest() const { return window_shown_ ? &*window_shown_ : nullptr; }
  /// THE HAND MOVED WITH A WIRE: past one character cell from the press it is a drag, drawn from
  /// the press to the hand, a port it would bind to lit.
  void wire_moved(const Moment &event, const ws::CanvasRoom &room, loom::Mail &mail) {
    const pane::Measure measure(room);
    const auto at = [](std::int64_t v) {
      return std::clamp(v, std::int64_t{-100000000}, std::int64_t{100000000});
    };
    const auto hx = at(event.x), hy = at(event.y);
    if (!drag_->crossed && std::abs(hx - drag_->x) < measure.ch && std::abs(hy - drag_->y) < measure.row)
      return;
    drag_->crossed = true;
    pane::WireDrawn drawn{drag_->x, drag_->y, hx, hy, {}};
    if (drag_->from.source)
      if (const auto *it = latest();
          it != nullptr && it->revision == model_.workspace.graph.project.definition.revision)
        if (const auto *over = it->hit(event.x, event.y);
            over != nullptr && over->action == "port" && over->args.size() == 2)
          drawn.lit = pane::PortChoice{over->subject, flow::index_of(over->args[1])};
    model_.wire_drawn = drawn;
    show(mail);
  }
  /// IS (x, y) EMPTY GRAPH ROOM in `it`: inside the graph's viewport, on the Graph page.
  static bool graph_room(const pane::Picture &it, std::int64_t x, std::int64_t y) {
    return it.canvas && x >= it.canvas->x && x < it.canvas->right() && y >= it.canvas->y &&
           y < it.canvas->bottom();
  }
  /// THE WIRE LET GO. Before the threshold, or back on its origin, it was a click, and what the
  /// press did stands. On the other end it binds; on empty graph room its search opens there;
  /// anywhere else the wire is put down and nothing is made.
  void let_go(const Drag &held, const Moment &event, loom::Mail &mail) {
    const bool drawn = model_.wire_drawn.has_value();
    model_.wire_drawn.reset();
    if (!held.crossed || event.phase != ws::canvas_pointer::kRelease) {
      if (drawn)
        show(mail);
      return;
    }
    const auto *it = latest();
    if (it == nullptr || it->revision != model_.workspace.graph.project.definition.revision) {
      model_.notice = "That picture has changed; choose again.";
      show(mail);
      return;
    }
    // A copy of what it was let go on: what it does clears the pictures it was read from.
    const auto landed = it->hit(event.x, event.y);
    const std::optional<pane::Hit> on = landed ? std::optional<pane::Hit>(*landed) : std::nullopt;
    const pane::Hit *hit = on ? &*on : nullptr;
    if (hit != nullptr && hit->action == held.origin && hit->args == held.origin_args) {
      show(mail);
      return;
    }
    if (held.from.source && hit != nullptr && hit->action == "port" && hit->args.size() == 2) {
      model_.connecting = *held.from.source;
      perform("port", hit->args, mail);
      return;
    }
    if (held.from.source && hit != nullptr && hit->action == "write") {
      model_.connecting = *held.from.source;
      perform("write", hit->args, mail);
      return;
    }
    if (held.from.port && hit != nullptr &&
        (hit->action == "source-node" || hit->action == "source-field" ||
         hit->action == "field" || hit->action == "scope")) {
      try {
        if (hit->action == "scope") {
          act("scope", hit->args, mail);
        } else {
          const auto port = filled(*held.from.port);
          act("bind", {std::to_string(port.first), std::to_string(port.second),
                       (hit->action == "source-node" ? "%" : "$") + hit->args.at(0)},
              mail);
        }
        model_.filling.reset();
      } catch (const std::exception &e) {
        model_.notice = e.what();
      }
      show(mail);
      return;
    }
    if (hit == nullptr && graph_room(*it, event.x, event.y)) {
      open_wire_search(held.from, event.x, event.y, event.x, event.y, *it, mail);
      return;
    }
    put_wire_down("let go off the graph");
    show(mail);
  }
  /// The node index and port a port's place names now, in the active trigger.
  std::pair<std::size_t, std::size_t> filled(const pane::PortChoice &port) const {
    for (const auto &p : model_.workspace.graph.places)
      if (p.id == port.place && p.trigger == model_.workspace.active_trigger)
        return {static_cast<std::size_t>(p.node), port.port};
    throw std::invalid_argument("That port's node is gone; nothing was wired");
  }
  /// OPEN A WIRE'S SEARCH from `from`, its list at (x, y) of the area and a node made from it
  /// placed at the graph's place under (px, py), as `it` draws the graph. The search line starts
  /// empty, and the door is asked afresh.
  void open_wire_search(const pane::WireEnd &from, std::int64_t x, std::int64_t y, std::int64_t px,
                        std::int64_t py, const pane::Picture &it, loom::Mail &mail) {
    if (from.source && !pane::source_type(model_, *from.source)) {
      put_wire_down("its type is not described here");
      show(mail);
      return;
    }
    pane::WireSearch search;
    search.from = from;
    search.x = x;
    search.y = y;
    std::tie(search.place_x, search.place_y) = pane::place_at(it, window_room_, px, py);
    search.revision = model_.workspace.graph.project.definition.revision;
    model_.wire_search = search;
    if (from.source)
      model_.connecting = from.source;
    else
      model_.filling = from.port;
    model_.search.set("", 0);
    model_.notice = "Choose what to make from the wire; Escape puts it down";
    find(mail, true);
    show(mail);
  }
  /// PUT A WIRE DOWN, and its search with it, the wire not left in hand; given why, the notice
  /// says nothing was made.
  void put_wire_down(const std::string &by = std::string()) {
    const bool held = model_.wire_search || model_.connecting || model_.filling;
    model_.wire_search.reset();
    model_.wire_drawn.reset();
    model_.connecting.reset();
    model_.filling.reset();
    if (held && !by.empty())
      model_.notice = "The wire was put down: " + by + "; nothing was made";
  }
  /// Move the keys to the next choosable row of a wire's search, down or up.
  void step_wire_choice(bool down) {
    const auto rows = pane::wire_rows(model_);
    const auto at = pane::wire_choice(model_, rows);
    if (!at)
      return;
    if (down) {
      for (auto i = *at + 1; i < rows.size(); ++i)
        if (!rows[i].kind.empty()) {
          model_.wire_search->selected = i;
          return;
        }
    } else {
      for (auto i = *at; i-- > 0;)
        if (!rows[i].kind.empty()) {
          model_.wire_search->selected = i;
          return;
        }
    }
    model_.wire_search->selected = *at;
  }
  /// MAKE ROW `i` OF A WIRE'S SEARCH, as one edit: a node placed where the hand let go, wired
  /// from the wire's output or into its port; an In scope row binds the port; the constant row
  /// opens the binding editor. A row gone, or a graph changed since the wire was let go, makes
  /// nothing.
  void choose_wire(std::size_t i, loom::Mail &mail) {
    if (!model_.wire_search)
      throw std::invalid_argument("No wire's search is open");
    const auto search = *model_.wire_search;
    if (search.revision != model_.workspace.graph.project.definition.revision) {
      put_wire_down("the graph changed");
      throw std::invalid_argument(
          "The graph changed since the wire was let go; nothing was made -- let it go again");
    }
    const auto rows = pane::wire_rows(model_);
    if (i >= rows.size() || rows[i].kind.empty())
      throw std::invalid_argument("That row is no longer in the list; choose again");
    const auto row = rows[i];
    const std::vector<std::string> place{std::to_string(search.place_x),
                                         std::to_string(search.place_y)};
    std::string said;
    if (search.from.source) {
      const auto source = flow::binding_text(*search.from.source);
      if (row.kind == "unread") {
        effect({pane::Effect::Catalog, {}}, mail);
        model_.notice =
            "Reading the ports of " + row.args.at(0) + " from the host; choose it again";
        return;
      }
      std::vector<std::string> args;
      std::string command = "add-fold-from";
      if (row.kind == "power") {
        command = "add-node-from";
        args = {row.args.at(0), row.args.at(1), row.args.at(2), source, row.args.at(3)};
      } else {
        args = {source, row.args.at(0)};
      }
      args.insert(args.end(), place.begin(), place.end());
      effect(model_.command(command, args), mail);
      said = ", wired from " + source;
    } else {
      const auto port = pane::wire_port(model_);
      if (!port) {
        put_wire_down("its port is gone");
        throw std::invalid_argument("That port is gone; nothing was made");
      }
      const auto n = std::to_string(port->node), a = std::to_string(port->port);
      if (row.kind == "scope") {
        effect(model_.command("bind", {n, a, row.args.at(0)}), mail);
        put_wire_down();
        model_.notice = "Bound %" + n + " " + port->field.name + " to " + row.args.at(0);
        return;
      }
      if (row.kind == "constant") {
        put_wire_down();
        act("constant", {n, a}, mail);
        return;
      }
      std::vector<std::string> args{row.args.at(0), row.args.at(1), row.args.at(2), n, a};
      args.insert(args.end(), place.begin(), place.end());
      if (!described(row.args.at(0))) {
        request(fh::FlowCatalog{}, "catalog", mail);
        adding_ = Adding{row.args.at(0), "add-node-into", std::move(args), requests_,
                         flow::workspace_bytes(model_.workspace)};
        put_wire_down();
        model_.notice = "Reading the ports of " + row.args.at(0) + " from the host";
        return;
      }
      effect(model_.command("add-node-into", args), mail);
      said = ", wired into %" + std::to_string(port->node + 1) + " " + port->field.name;
    }
    const auto added = model_.node;
    put_wire_down();
    model_.node = added;
    if (added)
      model_.notice = "Added %" + std::to_string(*added) + " " +
                      pane::node_title(model_.workspace.graph.project.definition
                                           .on.at(model_.trigger()).body.nodes.at(*added)) +
                      said;
  }
  /// `find-wire`: the same search, at the graph's centre, for a wire held by clicks or a port
  /// selected.
  void find_wire(loom::Mail &mail) {
    pane::WireEnd from;
    if (model_.connecting)
      from.source = model_.connecting;
    else if (model_.filling)
      from.port = model_.filling;
    else
      throw std::invalid_argument("Hold a wire first -- click an output or a field In scope, or "
                                  "select a port -- then ask for a new node from it");
    const auto &room = window_room_;
    if (!window_.shown() || room.width <= 0 || room.height <= 0 ||
        model_.page != pane::Page::Graph)
      throw std::invalid_argument("A new node from the wire needs the graph shown");
    const auto drawn = pane::picture(model_, room);
    if (!drawn.canvas)
      throw std::invalid_argument("A new node from the wire needs the graph shown");
    const pane::Measure measure(room);
    const auto cx = drawn.canvas->x + drawn.canvas->w / 2, cy = drawn.canvas->y + drawn.canvas->h / 2;
    open_wire_search(from, cx - measure.chars(18), cy - 3 * measure.row, cx, cy, drawn, mail);
  }
  /// The declared message a press landed on, in the picture it was aimed at: an accepted
  /// message's row or an emitted one's, on the Messages page.
  std::shared_ptr<const loom::Schema> pressed_message(const Moment &event,
                                                      const pane::Picture *it) const {
    if (it == nullptr || it->revision != model_.workspace.graph.project.definition.revision)
      return nullptr;
    const auto *hit = it->hit(event.x, event.y);
    if (!hit || hit->args.empty() ||
        (hit->action != "message-open" && hit->action != "emitted-row" && hit->action != "say"))
      return nullptr;
    const auto &def = model_.workspace.graph.project.definition;
    const auto at = flow::index_of(hit->args.front());
    if (hit->action == "say") {
      if (def.on.empty())
        return nullptr;
      const auto &on = def.on.at(model_.trigger());
      return at < on.emits.size() ? on.emits[at].message : nullptr;
    }
    if (hit->action == "message-open" && at < def.accepts.size())
      return def.accepts[at];
    if (hit->action == "emitted-row" && at < def.emits.size())
      return def.emits[at];
    return nullptr;
  }
  /// ASK TO CARRY A DECLARED MESSAGE'S SHAPE OUT as a description holding what it nests, under
  /// the gesture it continues: a press's drag, placed where it is released, or a menu choice's
  /// carry, placed by a click.
  void carry(const std::shared_ptr<const loom::Schema> &shape, std::uint64_t gesture, bool drag,
             loom::Mail &mail) {
    const auto bytes = zengine::inventory::encode_pair(flow::shape::carried(shape), {});
    FlowAsk carrying = asking(FlowAsk::Kind::kCarry);
    carrying.gesture = gesture;
    carrying.label = shape->name();
    carrying.bytes = loom::Bytes(bytes.begin(), bytes.end());
    carrying.drag = drag;
    (void)asks().forget(of_kind(FlowAsk::Kind::kCarry));
    (void)asks().ask(
        mail, workshop_role,
        ws::PaneOperationRequested{pane::kPane, workshop_role, ws::PaneValueCarryRequested::zen_name,
                                   1, static_cast<std::int64_t>(gesture)},
        std::move(carrying));
  }
  /// ASK WHETHER THE CONFIRMING GESTURE'S ACTOR MAY WRITE, before any byte: class `write` alone,
  /// since Flow writes the file itself. The verb and its values are kept as confirmed, and the
  /// dialog stays open until Workshop answers.
  void ask_to_write(const std::string &verb, std::vector<std::string> values,
                    loom::Mail &mail) {
    // A RAW KEY OR A WINDOW'S PRESS IS NO GESTURE WORKSHOP NUMBERED: a confirm that came as one
    // cannot be approved, so it writes nothing, keeps the dialog, and says which row confirms.
    if (mail.correlation() == 0)
      throw std::invalid_argument(
          pane::Model::writing(verb) +
          " waits: the confirm came under no gesture Workshop numbered, as a raw key or a press "
          "in a graphics window, which Workshop cannot approve -- confirm with the dialog's "
          "`dialog-confirm` row in Workshop");
    FlowAsk writing = asking(FlowAsk::Kind::kWrite);
    writing.action = verb;
    writing.values = std::move(values);
    if (asks().approve(mail, std::move(writing), mail.correlation(), {ws::scope::kWrite}) !=
        Asks::Asked::kHeld)
      throw std::invalid_argument(pane::Model::writing(verb) +
                                  " was not done: Workshop could not be asked");
    model_.notice = pane::Model::writing(verb) + " is waiting for Workshop's approval";
  }
  /// WORKSHOP'S WORD ON A CONFIRMED WRITE. Allowed, the verb runs with the values as confirmed
  /// and the dialog closes; refused, or failed, nothing is written and the dialog keeps its text
  /// for repair.
  void settle_write(const FlowAsk &writing, const ws::PaneOperationAnswered &answer,
                    loom::Mail &mail) {
    const auto doing = pane::Model::writing(writing.action);
    try {
      if (!answer.allowed)
        throw std::invalid_argument(doing + " was refused: " + answer.reason);
      if (!model_.dialog || model_.dialog->action != writing.action)
        throw std::invalid_argument(doing + " was not done: its dialog is no longer open");
      effect(model_.command(writing.action, writing.values), mail);
      model_.dialog.reset();
      drag_.reset();
    } catch (const std::exception &e) {
      model_.notice = e.what();
    }
    show(mail);
  }
  /// A dialog put down puts down the write it asked for: its answer, when it comes, finds none.
  void drop_writing() {
    if (const FlowAsk *dropped = asks().find(of_kind(FlowAsk::Kind::kWrite))) {
      model_.notice = pane::Model::writing(dropped->action) + " was cancelled with its dialog";
      (void)asks().forget(of_kind(FlowAsk::Kind::kWrite));
    }
  }
  std::shared_ptr<const loom::Schema> declared(const std::string &name) const {
    const auto &def = model_.workspace.graph.project.definition;
    for (const auto *list : {&def.accepts, &def.emits})
      for (const auto &shape : *list)
        if (shape->name() == name)
          return shape;
    return nullptr;
  }
  /// Bring the selected node into view in the room its area holds.
  void reveal() { pane::reveal_selected(model_, window_room_); }
  bool host(const loom::Mail &mail, const std::string &which) const {
    return which == pane::kPane && mail.authored_from_role(workshop_role);
  }
  /// The host request of `action` this answer settles, taken from the table; none for another.
  std::optional<FlowAsk> settle(loom::Mail &mail, const std::string &action) {
    return asks().answered(mail, [&action](const FlowAsk &a) {
      return a.kind == FlowAsk::Kind::kHost && a.action == action;
    });
  }
  template <class T>
  void request(const T &value, const std::string &action, loom::Mail &mail) {
    if (asks().count(of_kind(FlowAsk::Kind::kHost)) >= 16)
      throw std::invalid_argument(
          "Flow has 16 unanswered requests; inspect the host before retrying");
    FlowAsk asked = asking(FlowAsk::Kind::kHost);
    asked.action = action;
    asked.state_epoch = model_.state_epoch;
    asked.request = ++requests_;
    if (asks().ask(mail, fh::kFlowHostRole, value, std::move(asked)) != Asks::Asked::kHeld)
      throw std::invalid_argument(action + " could not be queued");
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
      find(mail, true);
      break;
    case pane::Effect::Inspect:
      request(fh::FlowInspect{"workshop", 0}, "inspect", mail);
      break;
    }
  }
  /// THE ROWS IN FORCE FOLLOW THE DIALOG: while one is open, Return is also its confirm, so the
  /// confirm arrives as an action under a gesture Workshop can approve, beside its cancel; Tab,
  /// Escape and a field's own keys still arrive as keys. Every other control Flow draws with no
  /// argument is a keyless row, so its `control:<action>` names an action in force (WL-HAND-07).
  /// The offer declares first; after it, declared again when the dialog opens or closes, or when
  /// `again` asks.
  void declare(loom::Mail &mail, bool again = false) {
    const bool dialog = model_.dialog.has_value();
    if (!again && (!declared_ || *declared_ == dialog))
      return;
    declared_ = dialog;
    std::vector<ws::v3::PaneActionRow> rows{
        {"ask-save", "Save Flow workspace", input::scan::kS, input::mod::kCtrl},
        {"ask-open", "Open Flow workspace", input::scan::kO, input::mod::kCtrl},
        {"run", "Run Flow", input::scan::kR, input::mod::kCtrl},
        {"ask-generate", "Generate native C++", input::scan::kG, input::mod::kCtrl},
        {"find-wire", "New node from the wire", input::scan::kN, input::mod::kCtrl},
        {"ask-discard", "Discard unsaved marker", 0, 0}};
    static constexpr std::pair<const char *, const char *> kControls[] = {
        {"ask-new", "New Flow workspace"},
        {"ask-import", "Import a Flow definition"},
        {"ask-export", "Export the Flow definition"},
        {"apply", "Apply to the running graph"},
        {"stop", "Stop the running graph"},
        {"restart", "Restart the running graph"},
        {"inspect", "Inspect the running graph"},
        {"catalog", "Read the catalog"},
        {"page-graph", "Show the graph"},
        {"page-state", "Show the state"},
        {"page-messages", "Show the messages"},
        {"page-library", "Show the library"},
        {"page-events", "Show the events"},
        {"fit", "Reset the view"},
        {"zoom-in", "Zoom in"},
        {"zoom-out", "Zoom out"},
        {"ask-trigger", "Add a trigger"},
        {"ask-emit", "Add an emit"},
        {"ask-keep", "Keep a field between messages"},
        {"ask-say", "Say a message after the change"},
        {"add-fold", "Add the form shown"},
        {"ask-state-field", "Add a state field"},
        {"keep-state", "Keep the state"},
        {"ask-message", "New message"},
        {"ask-message-field", "Add a message field"},
        {"ask-emitted-message", "New emitted message"},
        {"ask-emitted-field", "Add an emitted field"},
        {"ask-library-open", "Import into the library"},
        {"ask-library-save", "Export the library"},
        {"ask-preset", "Save the example"},
        {"send", "Send the example"},
        {"drop-send", "Send the dropped value"},
        {"drop-accept", "Declare it as accepted"},
        {"drop-emit", "Declare it as emitted"},
        {"drop-cancel", "Put the dropped value down"}};
    for (const auto &[id, label] : kControls)
      rows.push_back({id, label, 0, 0});
    if (dialog) {
      rows.push_back({"dialog-confirm", "Confirm the dialog", input::scan::kReturn, 0});
      rows.push_back({"dialog-cancel", "Cancel the dialog", 0, 0});
    }
    // WHAT A PRESS MAY SEND BESIDE THE PANE PROTOCOL: the host's ask its effect makes (`effect`),
    // and the door's question again where the press, or the host's answer to it, can change what
    // Flow asks (`find`).
    const ws::ActionCause found{ws::find_powers_schema()->name(),
                                static_cast<std::int64_t>(ws::find_powers_schema()->version()), {}};
    for (auto &row : rows) {
      if (row.id == "run")
        row.causes = {ws::cause_of<fh::FlowRun>(), found};
      else if (row.id == "apply")
        row.causes = {ws::cause_of<fh::FlowApply>(), found};
      else if (row.id == "stop")
        row.causes = {ws::cause_of<fh::FlowStop>(), found};
      else if (row.id == "restart")
        row.causes = {ws::cause_of<fh::FlowStop>(), ws::cause_of<fh::FlowRun>(), found};
      else if (row.id == "inspect")
        row.causes = {ws::cause_of<fh::FlowInspect>()};
      else if (row.id == "send" || row.id == "drop-send")
        row.causes = {ws::cause_of<fh::FlowSend>()};
      else if (row.id == "catalog")
        row.causes = {ws::cause_of<fh::FlowCatalog>(), found};
      else if (row.id == "add-fold" || row.id == "dialog-confirm" || row.id == "find-wire")
        row.causes = {found};
    }
    this->template facet<Actions>().declare(mail, std::move(rows));
  }
  void perform(const std::string &action, const std::vector<std::string> &args,
               loom::Mail &mail) {
    try {
      act(action, args, mail);
    } catch (const std::exception &e) {
      model_.notice = e.what();
      // RUN NEVER REFUSES BLIND: the Next line says what it needs first.
      if (action == "run")
        model_.nudge = true;
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
    drag_.reset();
    model_.wire_drawn.reset();
    if (model_.wire_search && action != "wire-row" && action != "wire-list")
      model_.wire_search.reset();
    if (action == "wire-list") {
      // A press inside a wire's list, on no row of it: nothing.
    } else if (action == "wire-row") {
      choose_wire(flow::index_of(args.at(0)), mail);
    } else if (action == "find-wire") {
      find_wire(mail);
    } else if (action == "ask-new")
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
    else if (action == "ask-generate")
      model_.ask("Generate native C++ for this definition into a directory", "generate",
                 {{"Directory", "flow-generated"}});
    else if (action == "ask-emitted-message")
      model_.ask("Declare a message this definition publishes", "emitted-message",
                 {{"Message name", "Said"}});
    else if (action == "ask-emitted-field")
      model_.ask("Add a field to an emitted message", "emitted-field",
                 {{"Emitted index", "0"}, {"Name", "value"}, {"Kind", "Int"},
                  {"Presence", "required"}});
    else if (action == "ask-emit") {
      // Each field written from the state field of its name, as the definition already holds
      // them; the maker edits the line before confirming.
      const auto &def = model_.workspace.graph.project.definition;
      std::string fields;
      if (!def.emits.empty())
        for (const auto &f : def.emits.front()->fields())
          if (def.state->find(f.name))
            fields += (fields.empty() ? "" : " ") + f.name + "=$" + f.name;
      model_.ask("After the write, publish an emitted message", "emit",
                 {{"Emitted index", "0"}, {"Fields (field=$state or field=constant)", fields}});
    }
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
    } else if (action == "dialog-cancel") {
      model_.dialog.reset();
      drop_writing();
    } else if (action == "dialog-confirm") {
      if (!model_.dialog)
        return;
      // ONE WRITE IN FLIGHT: a second confirm waits for the first one's answer.
      if (const FlowAsk *writing = asks().find(of_kind(FlowAsk::Kind::kWrite)))
        throw std::invalid_argument(pane::Model::writing(writing->action) +
                                    " is already waiting for Workshop's approval");
      const auto saved = *model_.dialog;
      std::vector<std::string> values;
      for (const auto &entry : saved.entries)
        values.push_back(entry.text.text());
      // A WRITE IS ASKED UNDER THE CONFIRMING GESTURE, and runs at Workshop's answer.
      if (pane::Model::writes_file(saved.action)) {
        ask_to_write(saved.action, std::move(values), mail);
        return;
      }
      // Refusal keeps the form and its authored text available for repair.
      if (saved.action == "discard")
        act("discard", values, mail);
      else
        effect(model_.command(saved.action, values), mail);
      model_.dialog.reset();
      if ((saved.action == "open" || saved.action == "import-project"))
        pane::reveal_graph(model_, window_room_);
      drag_.reset();
    } else if (action == "menu") {
      open_menu(args.at(0), mail);
    } else if (action == "restart") {
      // STOP, THEN RUN THE DRAFT: what it kept carries on where its shape is unchanged.
      if (!model_.running)
        throw std::invalid_argument("Nothing runs to restart; press Run");
      const auto errors = model_.workspace.graph.problems(model_.palette);
      if (!errors.empty())
        throw std::invalid_argument(errors.front());
      effect({pane::Effect::Stop, {}}, mail);
      restarting_ = true;
      model_.notice = "Restarting: stopping, then running the draft";
    } else if (action == "ask-keep") {
      model_.ask("Keep a field between messages: what the rule remembers", "keep",
                 {{"Name", "total"}, {"Kind", "Int"}});
    } else if (action == "ask-say") {
      const auto &def = model_.workspace.graph.project.definition;
      if (def.on.empty())
        throw std::invalid_argument(
            "A rule says a message after its change: give it a message and what it keeps first");
      const auto &on = def.on.at(model_.trigger());
      model_.ask("Say a message after the change, with what the rule keeps", "say",
                 {{"Message name", pane::say_name(on.output)}, {"With", on.output}});
    } else if (action == "write") {
      // A STEP'S ANSWER, held, given to what the rule writes: it becomes that answer.
      if (!model_.connecting || model_.connecting->from() != zengine::op::Binding::From::Node)
        throw std::invalid_argument("Press a step's answer, then this, to make the kept field "
                                    "that answer");
      const auto n = model_.connecting->node_index();
      effect(model_.command("result", {std::to_string(n)}), mail);
      model_.connecting.reset();
    } else if (action == "say") {
      // A SAY CARD PRESSED, whether a drag carries it on or not: where it goes to be shown.
      const auto &def = model_.workspace.graph.project.definition;
      const auto at = flow::index_of(args.at(0));
      std::string said = "it";
      if (!def.on.empty() && at < def.on.at(model_.trigger()).emits.size())
        said = def.on.at(model_.trigger()).emits[at].message->name();
      model_.notice = "Carry " + said + " onto a label to show it there: drag it, or right-press it";
    } else if (action == "drop-zone") {
      model_.notice = "Carry a button's message here, or press New message";
    } else if (action == "field") {
      model_.connecting = zengine::op::Binding::input(args.at(0));
      model_.notice = "Choose an input port to connect " + args.at(0);
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
        // The port to fill: the rail lists what could, and a second press puts it down.
        const pane::PortChoice chosen{
            model_.workspace.graph.place(model_.trigger(), flow::index_of(args.at(0))).id,
            flow::index_of(args.at(1))};
        const bool again = model_.filling && model_.filling->place == chosen.place &&
                           model_.filling->port == chosen.port;
        if (again)
          model_.filling.reset();
        else
          model_.filling = chosen;
      }
    } else if (action == "scope") {
      // What the pressed row named, in the picture it was pressed in: node, port and binding.
      effect(model_.command("bind", {args.at(0), args.at(1), args.at(2)}), mail);
      model_.filling.reset();
    } else if (action == "constant") {
      model_.ask(
          "Bind a constant, $field, or %earlier-node", "bind",
          {{"Node", args.at(0)}, {"Port", args.at(1)}, {"Value", "0"}});
      model_.dialog->selected = 2;
      model_.filling.reset();
    } else if (action == "found") {
      model_.preview_kind = args.at(0);
      model_.preview = args.at(1);
    } else if (action == "body-slot") {
      // The fold's body slot: the rail lists what a fold could spend, and a second press shuts it.
      const auto id =
          model_.workspace.graph.place(model_.trigger(), flow::index_of(args.at(0))).id;
      if (model_.body_slot == id)
        model_.body_slot.reset();
      else {
        model_.body_slot = id;
        model_.filling.reset();
      }
      model_.slot_reference.reset();
    } else if (action == "drop-send" || action == "drop-constant" || action == "drop-accept" ||
               action == "drop-emit" || action == "drop-cancel") {
      act_on_drop(action, args, mail);
    } else if (action == "add-found") {
      std::vector<std::string> before(args.begin() + 2, args.end());
      add_found(args.at(0), args.at(1), "add-node", std::move(before), mail);
      reveal();
    } else if (action == "add-into") {
      add_found(args.at(0), args.at(1), "add-node-into", {args.at(2), args.at(3)}, mail);
      reveal();
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
    } else if (action == "fold-body") {
      // A reference the slot offered and the graph then refused is put down: the slot offers the
      // door's row again, rather than the same refusal each time it is chosen.
      try {
        effect(model_.command(action, args), mail);
      } catch (const std::exception &) {
        const auto &held = model_.slot_reference;
        if (held && args.size() >= 4 && args[1] == held->ref.identity &&
            args[2] == std::to_string(static_cast<std::int64_t>(held->ref.authored_in)) &&
            args[3] == std::to_string(static_cast<std::int64_t>(held->ref.authored_out)))
          model_.slot_reference.reset();
        throw;
      }
      reveal();
    } else if (action == "emitted-row") {
      model_.notice = "Drag an emitted message, or right-press it, to carry its shape to another pane";
    } else if (action == "fit") {
      model_.workspace.pan_x = 0;
      model_.workspace.pan_y = -3 * pane::unit;
      model_.workspace.zoom = 75;
      pane::reveal_graph(model_, window_room_);
      model_.touched();
    } else if (action == "zoom-in" || action == "zoom-out") {
      model_.workspace.zoom =
          std::clamp(model_.workspace.zoom + (action == "zoom-in" ? 25 : -25),
                     std::int64_t{50}, std::int64_t{200});
      model_.touched();
    } else {
      // A VERB THAT WRITES A FILE IS NO BARE ACTION: only its dialog's confirm asks for it.
      if (pane::Model::writes_file(action))
        throw std::invalid_argument(pane::Model::writing(action) +
                                    " was refused: confirm it in its dialog");
      effect(model_.command(action, args), mail);
      drag_.reset();
    }
  }
  /// Ask the discovery door this pane's question as this office, when it differs from the last
  /// one asked or `again` wants a fresh look. A newer ask replaces the correlation, so the answer
  /// to a question the maker has moved past is dropped.
  void find(loom::Mail &mail, bool again = false) {
    loom::Value question = ws::find_powers_value(pane::flow_question(model_));
    std::string bytes = loom::serialize(question);
    if (!again && bytes == asked_)
      return;
    asked_ = std::move(bytes);
    (void)asks().forget(of_kind(FlowAsk::Kind::kFind));
    const auto shape = ws::find_powers_schema();
    (void)asks().ask_with(ws::kPowersRole, shape->name(), shape->version(),
                          asking(FlowAsk::Kind::kFind), [&](std::uint64_t correlation) {
                            return mail.bus().office_send_to_role(
                                pane::kRole, ws::kPowersRole,
                                loom::Message(std::move(question), self_, loom::WeaveId{},
                                              correlation));
                          });
  }
  bool described(const std::string &identity) const {
    return std::any_of(model_.palette.begin(), model_.palette.end(),
                       [&](const auto &ports) { return ports.identity == identity; });
  }
  /// Add a found power as a node: the operator reference the door's row carries -- its identity
  /// and the two content ids it was found at -- spent by `add-node` or `add-node-into` once the
  /// graph's ports describe it; a form's row places the form. One the last catalog answer did not
  /// hold is read from the host first and added when that answer comes, and a reference it then
  /// describes at other ports is refused, as is one whose graph changed meanwhile; the latest such
  /// Add is the one kept.
  void add_found(const std::string &kind, const std::string &identity, const std::string &command,
                 std::vector<std::string> where, loom::Mail &mail) {
    const auto row = std::find_if(model_.discovered.rows.begin(), model_.discovered.rows.end(),
                                  [&](const auto &r) { return r.kind == kind && r.identity == identity; });
    if (row == model_.discovered.rows.end())
      throw std::invalid_argument(identity + " is not among the powers the door last found");
    if (row->kind == ws::kFormKind) {
      effect(model_.command("add-fold", command == "add-node" ? where : std::vector<std::string>{}),
             mail);
      return;
    }
    std::vector<std::string> arguments{identity, std::to_string(row->inputs.content_id),
                                       std::to_string(row->outputs.content_id)};
    arguments.insert(arguments.end(), where.begin(), where.end());
    if (described(identity)) {
      effect(model_.command(command, arguments), mail);
      return;
    }
    request(fh::FlowCatalog{}, "catalog", mail);
    adding_ = Adding{identity, command, std::move(arguments), requests_,
                     flow::workspace_bytes(model_.workspace)};
    model_.notice = "Reading the ports of " + identity + " from the host";
  }
  /// A dropped operator reference becomes a node: into the port it landed on, or as the body of
  /// the fold whose slot it landed on (the slot opens with it found, for the maker to say which
  /// port takes the count, and the body is chosen from this reference), or else at the end of the
  /// trigger, placed where it was released.
  void drop_reference(const zengine::op::OperatorRef &ref, const pane::Hit *hit,
                      const pane::Picture &pictured, const ws::PaneCanvasValueDrop &drop,
                      loom::Mail &mail) {
    std::vector<std::string> arguments{ref.identity,
        std::to_string(static_cast<std::int64_t>(ref.authored_in)),
        std::to_string(static_cast<std::int64_t>(ref.authored_out))};
    if (model_.page != pane::Page::Graph || model_.workspace.graph.project.definition.on.empty())
      throw std::invalid_argument("Drop an operator on the graph of a trigger");
    if (hit && hit->action == "body-slot" && !hit->args.empty()) {
      const auto described = std::find_if(
          model_.palette.begin(), model_.palette.end(),
          [&](const auto &ports) { return ports.identity == ref.identity; });
      if (described != model_.palette.end() &&
          (described->inputs->content_id() != ref.authored_in ||
           described->outputs->content_id() != ref.authored_out))
        throw std::invalid_argument(zengine::op::reshaped_reason(ref.identity));
      model_.body_slot = hit->subject;
      model_.slot_reference = pane::SlotReference{hit->subject, ref};
      model_.filling.reset();
      model_.search.set(ref.identity, ref.identity.size());
      model_.preview = ref.identity;
      model_.preview_kind.clear();
      model_.notice = "Choose which port of " + ref.identity + " takes the count";
      return;
    }
    std::string command = "add-node";
    std::optional<std::pair<std::int64_t, std::int64_t>> landing;
    if (hit && hit->action == "port" && hit->args.size() == 2) {
      command = "add-node-into";
      arguments.push_back(hit->args[0]);
      arguments.push_back(hit->args[1]);
    } else if (pictured.canvas) {
      landing = pane::place_at(pictured, window_room_, drop.x, drop.y);
    }
    if (!described(ref.identity)) {
      request(fh::FlowCatalog{}, "catalog", mail);
      adding_ = Adding{ref.identity, command, std::move(arguments), requests_,
                       flow::workspace_bytes(model_.workspace)};
      model_.notice = "Reading the ports of " + ref.identity + " from the host";
      return;
    }
    effect(model_.command(command, arguments), mail);
    if (landing && model_.node)
      effect(model_.command("move", {std::to_string(*model_.node),
                                     std::to_string(std::clamp<std::int64_t>(landing->first, -10000000, 10000000)),
                                     std::to_string(std::clamp<std::int64_t>(landing->second, -10000000, 10000000))}),
             mail);
  }
  /// What the maker chose for the value on the drop page.
  void act_on_drop(const std::string &action, const std::vector<std::string> &args,
                   loom::Mail &mail) {
    if (!model_.dropped)
      throw std::invalid_argument("Nothing dropped is waiting");
    const auto dropped = *model_.dropped;
    if (action == "drop-cancel") {
      model_.dropped.reset();
      return;
    }
    if (action == "drop-send") {
      effect({pane::Effect::Send, flow::byte_vector(loom::serialize(dropped.value))}, mail);
      model_.dropped.reset();
      return;
    }
    if (action == "drop-constant") {
      pane::Model probe = model_;
      probe.filling = dropped.port;
      const auto port = pane::selected_port(probe);
      const auto *cell = dropped.value.get(args.at(0));
      if (!port || !cell)
        throw std::invalid_argument("That port or field is gone; drop the value again");
      effect(model_.command("bind", {std::to_string(port->node), std::to_string(port->port),
                                     raw(*cell)}),
             mail);
      model_.dropped.reset();
      return;
    }
    const auto shape = pane::dropped_shape(dropped.value);
    if (!shape)
      throw std::invalid_argument("That description names shapes it does not carry");
    model_.declare(shape, action == "drop-emit");
    model_.dropped.reset();
    model_.notice = "Declared " + shape->name() +
                    (action == "drop-emit" ? " as an emitted message"
                                           : " as an accepted message; a trigger on it wakes the rule");
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
  /// SHOW WHAT IT HOLDS NOW in its area's graphics window.
  void show(loom::Mail &mail) {
    find(mail);
    declare(mail);
    show_window(mail);
  }
  /// The window that shows Flow's area is sent what changed there (nothing when nothing did), or
  /// the picture whole.
  void show_window(loom::Mail &mail) {
    if (!window_.shown() || window_.width() <= 0 || window_.height() <= 0)
      return;
    window_room_ = ws::canvas_window_room(window_.width(), window_.height(), window_.cell_px(),
                                          window_.text());
    auto drawn = pane::window_picture(model_, window_room_);
    window_parts_ = std::move(drawn.parts);
    window_shown_ = std::move(drawn.picture);
    drawn.sketch.number(dialog_number());
    window_.draw(drawn.sketch, mail);
  }
  /// WHAT FLOW'S DRAWING IS NUMBERED BY: the dialog it shows, by what it asks and does; else 0.
  std::int64_t dialog_number() const {
    if (!model_.dialog)
      return 0;
    ws::Fingerprint f;
    f.bytes(model_.dialog->action);
    f.bytes(model_.dialog->title);
    return f.value() | 1;
  }
  pane::Model model_;
  Room &room() { return this->template facet<Room>(); }
  const Room &room() const { return this->template facet<Room>(); }
  Asks &asks() { return this->template facet<Asks>(); }
  /// This image's one outstanding menu (`PaneMenuFacet`).
  Menu &menu() { return this->template facet<Menu>(); }
  bool restored_ = false;
  /// The number of the last host request asked, which an Add names to wait on its answer.
  std::uint64_t requests_ = 0;
  std::int64_t observed_pending_ = 0;
  std::optional<zengine::message_draft::Draft> live_state_;
  std::optional<Drag> drag_;
  ws::WindowDrawing window_{pane::kRole, ws::pane_area(pane::kRole, pane::kPane)}; ///< its area
  std::map<std::string, pane::Hit> window_parts_; ///< the parts the picture last sent there names
  std::optional<pane::Picture> window_shown_;     ///< the picture last sent there, and its room
  ws::CanvasRoom window_room_;
  bool room_asked_ = false;                       ///< its area's room asked in this life
  bool restarting_ = false; ///< a Restart's stop is asked: its answer runs the draft
  zengine::component::Clipboard clipboard_;
  /// An Add waiting on the host request numbered `request` -- the catalog request that will
  /// describe what it adds -- its node and port numbers meaning what they meant in `draft`.
  struct Adding {
    std::string identity, command;
    std::vector<std::string> arguments;
    std::uint64_t request = 0;
    std::string draft;
  };
  /// The bytes of the last question asked of the discovery door.
  std::string asked_;
  std::optional<Adding> adding_;
  /// Whether the rows last declared held the dialog's confirm; none before the offer's.
  std::optional<bool> declared_;
};
} // namespace
ZEN_EXPORT_WEAVE(FlowPane)
