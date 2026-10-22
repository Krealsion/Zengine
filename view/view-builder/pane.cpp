// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
// The View Builder: a pane beside Flow where a view is made by hand on a design canvas the view
// draws itself on, and run through the view host. Law: agents/view.md. Manual:
// view/docs/zengine.view.builder.md.
#include "app/folder.hpp"
#include "app/program.hpp"
#include "launcher/vocabulary.hpp"
#include "view/view-builder/picture.hpp"
#include "view/view-builder/vocabulary.hpp"
#include "view/vocabulary.hpp"

#include "graphics/canvas.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "inventory/codec.hpp"
#include "message-draft/transfer.hpp"
#include "workshop/actor_scope.hpp"
#include "workshop/pane_canvas_drawing.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_drawing.hpp"
#include "workshop/pane_facets.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_settings.hpp"
#include "workshop/pane_settings_taken.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "manuals/zengine.view.builder.hpp" // this pane's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/kernel/schema_codec.hpp>
#include <zen/registry.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace {
namespace vb = zengine::view_builder;
namespace view = zengine::view;
namespace ws = zengine::workshop;
namespace input = zengine::input;
namespace gfx = zengine::graphics;
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

/// ONE THING THE BUILDER ASKED AND STILL WAITS ON (`AskFacet`): where the project is; a request
/// of the view host, by its action, and whether it asks again what a reload carried; a save, with
/// the file File named when it was asked; an export, with the view's file it makes the app from
/// and whether the weaver pressed again to replace an app already there; a start of the app, by
/// its name, first asked of Workshop and then of the launcher; or an intent's shape being carried
/// out, with the gesture it continues and the bytes Workshop will carry.
struct ViewBuilderAsk {
    enum class Kind { kRoot, kHost, kSave, kExport, kStart, kLaunch, kCarry };
    Kind kind = Kind::kHost;
    std::string action; ///< the host request's action
    bool again = false; ///< a resume asking whether the view a reload carried as running still runs
    std::string path;   ///< the file a save writes, the view's file an export makes its app from,
                        ///< or the app a start names
    bool replace = false; ///< an export: the app's folder was there, and the weaver pressed again
    std::uint64_t gesture = 0;
    std::string label;
    loom::Bytes bytes;
    bool drag = false;
};

ViewBuilderAsk asking(ViewBuilderAsk::Kind kind) {
    ViewBuilderAsk a;
    a.kind = kind;
    return a;
}

auto of_kind(ViewBuilderAsk::Kind kind) {
    return [kind](const ViewBuilderAsk& a) { return a.kind == kind; };
}

/// The view host's words when this office's session already runs a view: a resume asked to learn
/// whether it still runs hears them as yes.
constexpr std::string_view kAlreadyRuns = "this session already runs a view";

using Room = ws::PaneFacet<vb::kPane, vb::kRole, zengine::manual::pages::zengine_view_builder::kManual,
                           ws::v3::PaneOffered>;
using Asks = ws::AskFacet<vb::kPane, vb::kRole, ViewBuilderAsk>;
using Actions = ws::PaneActionFacet<vb::kPane, vb::kRole>;
using Menu = ws::PaneMenuFacet<vb::kPane, vb::kRole>;

class ViewBuilderPane final
    : public zengine::With<Room, Asks, Actions, Menu>::Weave<
          ViewBuilderPane, vb::ViewBuilderState,
          loom::Accept<ws::PaneCanvasPointer, ws::PaneCanvasHover, ws::PaneKey, ws::PaneTextInput,
                       ws::ActionsJudged, ws::PaneQuitRequested, ws::PaneCanvasValueDrop,
                       ws::PaneCarryAnswered, vb::ViewEdit, view::ViewAnswer, ws::ProjectRoot,
                       zengine::launcher::Launched,
                       gfx::AreaRoom, gfx::DrawnRefused, gfx::ActionRequested, gfx::AreaButtonChanged,
                       gfx::AreaPointerMoved, gfx::AreaWheelTurned, gfx::AreaHoldEnded, gfx::AreaPointerLeft,
                       ws::v2::PaneSettings>,
          loom::Emit<ws::PaneContent, ws::v4::PaneCanvasContent, ws::PaneEscapeUnspent,
                     ws::PanePassRequested, ws::PaneQuitAnswered, ws::PaneOperationRequested,
                     ws::PaneValueCarryRequested, vb::ViewEdited, view::ViewRun, view::ViewResume,
                     zengine::launcher::Launch,
                     view::ViewApply, view::ViewStop, ws::ProjectRootRequested, gfx::Drawn,
                     gfx::RoomAsked, ws::v2::PaneSettingsDeclared, ws::PaneSettingUnused>> {
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

    void on_activated(loom::Mail& mail) {
        // A SAVE STILL WAITING ON WORKSHOP ends with this activation, and is said: its answer finds
        // nothing waiting, and a successor never holds one.
        if (asks().forget(of_kind(ViewBuilderAsk::Kind::kSave)) > 0)
            model_.notice = "The save waiting for Workshop was dropped; save again";
        if (asks().forget(of_kind(ViewBuilderAsk::Kind::kExport)) > 0)
            model_.notice = "The export waiting for Workshop was dropped; export again";
        if (asks().forget(of_kind(ViewBuilderAsk::Kind::kStart)) +
                asks().forget(of_kind(ViewBuilderAsk::Kind::kLaunch)) > 0)
            model_.notice = "The app's start waiting for an answer was dropped; run it again";
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
            (void)asks().ask(mail, ws::kProjectRole, ws::ProjectRootRequested{},
                             asking(ViewBuilderAsk::Kind::kRoot));
            // A VIEW A RELOAD CARRIED AS RUNNING IS THE VIEW HOST'S FACT, asked again: still
            // running, the host refuses to resume it; stopped meanwhile, it runs again, as a launch
            // runs what the builder ran.
            if (model_.running) ask_running(mail);
        }
        room().announce(*this, mail);
        // ...and its area's room, once a life: a builder started again draws in its window again.
        if (!room_asked_ && !window_.shown()) {
            room_asked_ = true;
            (void)mail.as_role(vb::kRole).send_to_role(gfx::kGraphicsRole, gfx::RoomAsked{window_.area()});
        }
    }
    ws::v3::PaneOffered offering() const {
        return ws::v3::PaneOffered{vb::kPane, "View Builder", "make a view by hand, then run it beside Flow",
                                   880, 540, 0};
    }
    /// The rows the pane answers to, and the settings it takes.
    void offered(loom::Mail& mail) {
        declare(mail);
        (void)mail.as_role(vb::kRole)
            .send_to_role(workshop_role, ws::v2::PaneSettingsDeclared{vb::kPane, setting_rows()});
    }
    /// THE SETTINGS THIS LAYOUT KEEPS FOR THE PANE, taken whole; what it cannot use is said back
    /// against the revision it was handed at. A hand-off moves no view: the size is the next
    /// `new`'s, the reach the next placing hand's. Held as plain members, never reload state.
    void on(const ws::v2::PaneSettings& handed, loom::Mail& mail) {
        if (!mail.authored_from_role(workshop_role) || handed.pane != vb::kPane) return;
        const ws::SettingsTaken taken = ws::take_pane_settings(setting_rows(), handed);
        ws::say_settings_unused(mail, vb::kRole, taken);
        snap_reach_ = taken.number(vb::kSettingSnapReach);
        model_.new_width = taken.number(vb::kSettingNewWidth);
        model_.new_height = taken.number(vb::kSettingNewHeight);
    }
    /// THE SETTINGS THE PANE TAKES, each with its default, how a weaver finds it and what it does.
    static std::vector<ws::v2::PaneSettingRow> setting_rows() {
        ws::v2::PaneSettingRow reach;
        reach.key = vb::kSettingSnapReach;
        reach.number = vb::kSnapReach;
        reach.low = 0;
        reach.high = vb::kMostSnapReach;
        reach.tags = {"layout", "input"};
        reach.pinned = true;
        reach.summary = "snap reach in pixels when dragging";
        ws::v2::PaneSettingRow width;
        width.key = vb::kSettingNewWidth;
        width.number = vb::kNewViewWidth;
        width.low = view::kMinWidthPx;
        width.high = view::kMaxPixels;
        width.tags = {"layout"};
        width.summary = "initial width in pixels of a new view";
        ws::v2::PaneSettingRow height;
        height.key = vb::kSettingNewHeight;
        height.number = vb::kNewViewHeight;
        height.low = view::kMinHeightPx;
        height.high = view::kMaxPixels;
        height.tags = {"layout"};
        height.summary = "initial height in pixels of a new view";
        return {reach, width, height};
    }
    void on_room(loom::Mail& mail) { show(mail); }
    void on_canvas_room(loom::Mail& mail) {
        // A fresh room ends whatever the old one held: a drag, a hover, a mark. The pan stays,
        // held within what the new room reaches.
        pictures_.clear();
        held_.reset();
        shown_.hovered.reset();
        shown_.landing.reset();
        shown_.ghost.reset();
        shown_.met_x.reset();
        shown_.met_y.reset();
        show(mail);
    }
    void on_rejected(const ws::PaneCanvasRejected& answer, loom::Mail&) {
        if (answer.grant == canvas_room().grant) model_.notice = "Drawing rejected: " + answer.reason;
    }
    /// WHERE THE PROJECT IS: where this builder's own file lives, what that file holds, read
    /// rather than assumed, and, at a launch, what it runs. A successor writes there at once what
    /// its predecessor could not.
    void on(const ws::ProjectRoot& said, loom::Mail& mail) {
        if (!asks().answered(mail, of_kind(ViewBuilderAsk::Kind::kRoot))) return;
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
    void on(const ws::ActionsJudged& answer, loom::Mail& mail) {
        if (host(mail, answer.pane) && !answer.accepted) {
            model_.notice = "View Builder actions: " + answer.refusal;
            show(mail);
        }
    }
    void on_action(const std::string& id, loom::Mail& mail) {
        if (shown_.box && !commit()) {
            show(mail);
            return;
        }
        // A row's id is `view.` and the control it presses.
        const std::string control = id.rfind("view.", 0) == 0 ? id.substr(5) : std::string();
        if (control == "save") ask_save(mail);
        else if (control == "export") ask_export(mail);
        else if (control == "run-app") ask_start(mail);
        else if (control == "choices") more_choices();
        else if (control == "open" || control == "run" || control == "new" || control == "apply" || control == "stop")
            perform(control, {}, mail);
        show(mail);
    }
    void on(const ws::PaneQuitRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(workshop_role)) return;
        const std::string why = asks().holds(of_kind(ViewBuilderAsk::Kind::kHost))
                                    ? "The View Builder is waiting for the view host. Quit again after it answers."
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
        if (!host(mail, event.pane) || event.grant != canvas_room().grant) return;
        pointer(event, mail);
    }
    /// THE POINTER IN THE WINDOW, given by the window that shows the builder's area as its office:
    /// what it is on Workshop's canvas, read against the picture last drawn there and the room it
    /// was drawn in, a press's moves and release under the window's number for it, a hold the window
    /// ended a lost press. A move with no press held marks what the pointer rests on, and the
    /// pointer leaving the area puts the mark down.
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) { on_window(window_.pointer(b, mail, 0, 0), mail); }
    void on(const gfx::AreaPointerMoved& m, loom::Mail& mail) {
        const auto p = window_.pointer(m, mail, 0, 0);
        if (p && p->press == 0 && !held_) {
            rest(true, p->x, p->y, window_shown_ ? &*window_shown_ : nullptr, false, mail);
            return;
        }
        on_window(p, mail);
    }
    void on(const gfx::AreaWheelTurned& w, loom::Mail& mail) { on_window(window_.pointer(w, mail, 0, 0), mail); }
    void on(const gfx::AreaHoldEnded& e, loom::Mail& mail) { on_window(window_.pointer(e, mail), mail); }
    void on(const gfx::AreaPointerLeft& left, loom::Mail& mail) {
        if (window_.from_window(left.area, mail)) rest(false, 0, 0, nullptr, false, mail);
    }
    void on_window(const std::optional<ws::WindowPointer>& p, loom::Mail& mail) {
        if (!p) return;
        if (p->phase == ws::canvas_pointer::kPress) rest(false, 0, 0, nullptr, false, mail);
        const std::int64_t gesture = p->phase == ws::canvas_pointer::kLost
                                         ? (held_ ? held_->gesture : 0)
                                         : static_cast<std::int64_t>(p->press);
        const ws::PaneCanvasPointer event{vb::kPane, 0,    0,    gesture,      p->phase,
                                          p->button, p->x, p->y, p->modifiers, p->dx,
                                          p->dy,     false};
        struct InWindow {
            bool& on;
            ~InWindow() { on = false; }
        } in_window{window_hand_};
        window_hand_ = true;
        pointer(event, mail);
    }
    /// ONE MOMENT OF THE POINTER, on either medium (`aimed`, `hand_room`).
    void pointer(const ws::PaneCanvasPointer& event, loom::Mail& mail) {
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
        if (!host(mail, hover.pane) || hover.grant != canvas_room().grant) return;
        const vb::Picture* picture = nullptr;
        if (!pictures_.empty()) {
            const auto pictured = std::find_if(pictures_.begin(), pictures_.end(),
                                               [&](const auto& p) { return p.content.picture == hover.picture; });
            picture = pictured == pictures_.end() ? &pictures_.back() : &*pictured;
        }
        rest(hover.over, hover.x, hover.y, picture, hover.carrying, mail);
    }
    /// WHERE THE POINTER RESTS, over `picture` at a point, or nowhere: the mark, as both media say.
    void rest(bool over_canvas, std::int64_t x, std::int64_t y, const vb::Picture* picture,
              bool carrying, loom::Mail& mail) {
        std::optional<std::size_t> over, landing;
        if (over_canvas && picture != nullptr) {
            if (const auto* hit = picture->hit(x, y)) over = element_of(*hit);
            if (over && carrying) {
                if (model_.description.elements.at(*over).kind == view::Kind::label) landing = over;
                over.reset();
            }
        }
        if (over == shown_.hovered && landing == shown_.landing) return;
        shown_.hovered = over;
        shown_.landing = landing;
        show(mail);
    }
    /// THE ROOM, from the graphics window that shows the builder's area, as that window's office: a
    /// window, an incarnation or a size it has not drawn in holds none of its picture, so it goes
    /// whole.
    void on(const gfx::AreaRoom& r, loom::Mail& mail) {
        if (window_.room(r, mail)) {
            window_parts_.clear();
            show_window(mail);
        }
    }
    /// A REFUSED CHANGE is drawn again whole at once, against nothing the window lacks; a refused
    /// whole picture is said in the notice and nothing goes there until the picture changes, so a
    /// window that keeps refusing starts no loop (`ws::WindowDrawing::refused`).
    void on(const gfx::DrawnRefused& refused, loom::Mail& mail) {
        if (!window_.refused(refused, mail) || !window_.refused_whole()) return;
        model_.notice = "The graphics window refused the View Builder's picture: " + refused.reason;
        show_workshop(mail);
    }
    /// A PRESS ASKED AS A PART'S ACTION, by a window that shows the builder's area placed deaf to
    /// the pointer: what a first-button press on that place of Workshop's canvas does, let go
    /// without moving (`choose`).
    void on(const gfx::ActionRequested& asked, loom::Mail& mail) {
        if (!window_.from_window(asked.area, mail)) return;
        const auto part = window_parts_.find(asked.action);
        if (part == window_parts_.end()) return;
        choose(part->second, nullptr, {}, {}, mail);
    }
    /// A SHAPE OR A FIELD CARRIED HERE, placed on a label on the canvas or its row: the label shows
    /// it. Placed anywhere else it is refused in words, whatever is selected. Carrying it granted
    /// nothing, and nothing else in the builder changes.
    void on(const ws::PaneCanvasValueDrop& drop, loom::Mail& mail) {
        if (!host(mail, drop.pane) || drop.grant != canvas_room().grant) return;
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
    void on_menu(const std::string& id, const std::string& subject, loom::Mail& mail) {
        if (id != "carry") return;
        // The subject is still a shape this view says, or nothing is carried.
        const auto intent = std::find_if(model_.description.intents.begin(), model_.description.intents.end(),
                                         [&](const auto& i) { return i.shape->name() == subject; });
        if (intent == model_.description.intents.end()) {
            model_.notice = subject + " is no longer said here; nothing was carried";
            show(mail);
            return;
        }
        carry(intent->shape, mail.correlation(), false, mail);
    }
    /// WORKSHOP'S WORD ON A SAVE, an export or a carry: each is the exact ask `AskFacet` held.
    void on_approval(const ViewBuilderAsk& asked, const ws::PaneOperationAnswered& answer, loom::Mail& mail) {
        if (asked.kind == ViewBuilderAsk::Kind::kExport) {
            if (!answer.allowed) model_.notice = "Not exported: " + answer.reason;
            else export_app(asked);
            show(mail);
            return;
        }
        if (asked.kind == ViewBuilderAsk::Kind::kStart) {
            if (!answer.allowed) model_.notice = "Not started: " + answer.reason;
            else launch(asked.path, mail);
            show(mail);
            return;
        }
        if (asked.kind == ViewBuilderAsk::Kind::kSave) {
            // ALLOWED, THE SAVE WRITES THE FILE NAMED WHEN IT WAS ASKED, and is remembered as any
            // save is, while File still names that file; refused, the refusal is said, and nothing
            // is written or remembered.
            if (!answer.allowed) model_.notice = "Saving was refused: " + answer.reason;
            else if (model_.path != asked.path)
                model_.notice = "Not saved: File no longer names " + asked.path + "; save again";
            else perform("save", {asked.path}, mail);
            show(mail);
            return;
        }
        if (asked.kind != ViewBuilderAsk::Kind::kCarry) return;
        if (!answer.allowed) {
            model_.notice = "Carrying " + asked.label + " was refused: " + answer.reason;
            show(mail);
            return;
        }
        (void)mail.as_role(vb::kRole).send_to_role(
            workshop_role, ws::PaneValueCarryRequested{vb::kPane, asked.label, asked.bytes, asked.drag},
            asked.gesture);
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
        const auto asked = asks().answered(mail, [&](const ViewBuilderAsk& a) {
            return a.kind == ViewBuilderAsk::Kind::kHost && a.action == answer.action;
        });
        if (!asked) return;
        if (asked->again) {
            // WHETHER THE VIEW A RELOAD CARRIED STILL RUNS, as the host says it: refused because
            // it runs, it runs; resumed, it runs again; refused otherwise, it does not run.
            const bool runs = !answer.ok && answer.reason.rfind(kAlreadyRuns, 0) == 0;
            model_.running = answer.ok || runs;
            model_.notice = runs        ? "The view host still runs " + model_.description.name
                            : answer.ok ? "The view had stopped; " + answer.reason
                                        : "The view had stopped and could not run again: " + answer.reason;
        } else if (answer.ok) {
            model_.running = answer.action != "stop";
            model_.notice = answer.reason;
        } else {
            model_.notice = answer.action + " refused: " + answer.reason;
        }
        remember();
        show(mail);
    }
    /// THE LAUNCHER'S ANSWER to this builder's own start: the process it started, or why not.
    void on(const zengine::launcher::Launched& answer, loom::Mail& mail) {
        if (!asks().answered(mail, of_kind(ViewBuilderAsk::Kind::kLaunch))) return;
        model_.notice = answer.ok ? "Started the app " + answer.name + " (process " + std::to_string(answer.pid) +
                                        "): " + zengine::launcher::kStartedAppGoesOn
                                  : "Not started: " + answer.reason;
        show(mail);
    }
    /// LOOM'S WORD THAT EXACTLY ONE OF THE BUILDER'S ASKS WAS REFUSED BEFORE ANY HANDLER RAN,
    /// matched by `AskFacet`: a save Workshop never heard, or a request of the view host. The
    /// project's and a carry's refusals say nothing.
    void on_refused(const ViewBuilderAsk& asked, const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (asked.kind == ViewBuilderAsk::Kind::kSave)
            model_.notice = "Not saved: Workshop could not be asked to allow it (" + refused.reason + ")";
        else if (asked.kind == ViewBuilderAsk::Kind::kExport)
            model_.notice = "Not exported: Workshop could not be asked to allow it (" + refused.reason + ")";
        else if (asked.kind == ViewBuilderAsk::Kind::kStart)
            model_.notice = "Not started: Workshop could not be asked to allow it (" + refused.reason + ")";
        else if (asked.kind == ViewBuilderAsk::Kind::kLaunch)
            model_.notice = "Not started: the launcher could not be asked (" + refused.reason + ")";
        else if (asked.kind == ViewBuilderAsk::Kind::kHost)
            model_.notice = asked.action + " was not delivered: " + refused.reason;
        else
            return;
        show(mail);
    }

private:
    static constexpr std::size_t kMaxTyped = 4096;
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
        const vb::Picture* pictured = aimed(event.picture);
        const auto* hit = pictured == nullptr ? nullptr : pictured->hit(event.x, event.y);
        if (event.button == 3) {
            // A RIGHT PRESS ON WHAT A BUTTON SAYS offers to carry its intent's shape; anywhere else
            // it means nothing here, and is handed back for Workshop's pane menu.
            const auto at = hit && hit->action == "says" ? element_of(*hit) : std::nullopt;
            const auto* intent = at ? model_.description.intent(model_.description.elements[*at].id) : nullptr;
            if (!intent) {
                (void)ws::pane_menu::pass_back(mail, vb::kRole, event.pane);
                return;
            }
            menu().send(mail, menu().offer(intent->shape->name())
                                  .row("carry", "Carry " + intent->shape->name()));
            return;
        }
        if (event.button == 2) {
            // A MIDDLE PRESS ON THE DESIGN CANVAS pans it while the hand holds it.
            if (pictured != nullptr && pictured->design.contains(event.x, event.y, hand_room().grain))
                held_ = Held{.gesture = event.gesture, .what = Held::What::pan, .x = event.x, .y = event.y,
                             .design = pictured->design, .pan_x = shown_.pan_x,
                             .pan_y = shown_.pan_y};
            return;
        }
        if (event.button != 1) return;
        if (pictured == nullptr) {
            model_.notice = "That picture has changed; choose again.";
            show(mail);
            return;
        }
        if (!hit) return;
        choose(*hit, &event, pictured->design, pictured->view, mail);
    }

    /// A FIRST-BUTTON PRESS ON A PLACE THE BUILDER ANSWERS: on Workshop's canvas, `event`, which
    /// may begin a drag; in a graphics window, none, so what a press there does is what the same
    /// press let go without moving does -- a kind made, an element selected -- and a size handle,
    /// which only a drag moves, does nothing. Each is taken whole, since acting clears the pictures
    /// the hit and its areas were read from.
    void choose(vb::Hit chosen, const ws::PaneCanvasPointer* event, vb::Area design, vb::Area placed,
                loom::Mail& mail) {
        // A value being typed is kept before anything else is pressed; a refusal keeps its box.
        if (shown_.box && !(chosen.action == "box" && same_box(chosen)) && !commit()) {
            show(mail);
            return;
        }
        if (chosen.action != "new" && chosen.action != "open" && chosen.action != "export")
            shown_.armed.clear();
        const auto index = element_of(chosen);
        if (chosen.action == "box") {
            if (!shown_.box || !same_box(chosen)) focus(chosen);
            if (event) place_caret(chosen, *event);
        } else if (chosen.action == "kind") {
            const auto kind = *view::kind_of(chosen.args.at(0));
            if (!event) perform("add", {view::kind_word(kind)}, mail);
            else
                held_ = Held{.gesture = event->gesture, .what = Held::What::make, .kind = kind, .x = event->x,
                             .y = event->y, .dirty = model_.dirty, .design = design, .placed = placed};
        } else if (chosen.action == "element" || chosen.action == "handle") {
            perform("select", {std::to_string(*index)}, mail);
            const bool handle = chosen.action == "handle";
            if (event)
                held_ = Held{.gesture = event->gesture, .what = handle ? Held::What::resize : Held::What::move,
                             .element = *index, .sx = handle ? std::stoi(chosen.args.at(1)) : 0,
                             .sy = handle ? std::stoi(chosen.args.at(2)) : 0, .x = event->x, .y = event->y,
                             .before = model_.description.elements[*index], .dirty = model_.dirty,
                             .design = design, .placed = placed};
        } else if (chosen.action == "size") {
            if (event)
                held_ = Held{.gesture = event->gesture, .what = Held::What::size, .sx = std::stoi(chosen.args.at(0)),
                             .sy = std::stoi(chosen.args.at(1)), .x = event->x, .y = event->y,
                             .dirty = model_.dirty, .design = design, .placed = placed,
                             .width = model_.description.width, .height = model_.description.height};
        } else if (chosen.action == "says") {
            // A PRESS ON WHAT A BUTTON SAYS may drag its intent's shape out: asked under the press,
            // carried only if the hand moves before it lets go.
            perform("select", {std::to_string(*index)}, mail);
            if (const auto* intent = model_.description.intent(model_.description.elements[*index].id);
                intent && event)
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
        } else if (chosen.action == "export") {
            ask_export(mail);
        } else if (chosen.action == "run-app") {
            ask_start(mail);
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
            const auto [reach_x, reach_y] = vb::pan_reach(model_.description, h.design, view::notice_band(hand_room()));
            const auto x = std::clamp<std::int64_t>(h.pan_x - dx, 0, reach_x);
            const auto y = std::clamp<std::int64_t>(h.pan_y - dy, 0, reach_y);
            if (x == shown_.pan_x && y == shown_.pan_y) return;
            shown_.pan_x = x;
            shown_.pan_y = y;
            show(mail);
            return;
        }
        const auto threshold = std::max<std::int64_t>(hand_room().grain, 4);
        if (!h.moved && std::abs(dx) < threshold && std::abs(dy) < threshold) return;
        h.moved = true;
        if (h.what == Held::What::make) {
            shown_.ghost.reset();
            shown_.met_x.reset();
            shown_.met_y.reset();
            if (h.design.contains(event.x, event.y, hand_room().grain)) {
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
                                                 snapping && h.sy != 0, shown_.grid, snap_reach_);
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
        const auto snapped =
            snapping ? vb::snap(model_.description, h.element, at, along_x, along_y, shown_.grid, snap_reach_) : at;
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
        return vb::snap(model_.description, std::nullopt, under, vb::Edges::both, vb::Edges::both, shown_.grid,
                        snap_reach_);
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
            } else if (released && h.design.contains(event.x, event.y, hand_room().grain)) {
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
        const vb::Picture* last = window_hand_ ? aimed(0) : pictures_.empty() ? nullptr : &pictures_.back();
        if (last == nullptr || event.x >= last->design.x || !std::isfinite(event.dy)) return;
        const auto rows = static_cast<std::int64_t>(model_.first_row) - static_cast<std::int64_t>(std::clamp(event.dy, -10.0, 10.0));
        model_.first_row = static_cast<std::size_t>(std::clamp<std::int64_t>(rows, 0, static_cast<std::int64_t>(view::kMaxElements)));
        model_.selected.reset();
        show(mail);
    }

    /// Arrow keys move the selected element a pixel, or a cell with Shift; one that would cross
    /// the view's edge is refused in words.
    bool nudge(const ws::PaneKey& key) {
        const std::int64_t step = (key.modifiers & input::mod::kShift) != 0 ? gfx::kCellPx : 1;
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
    /// A box opened on its value as it stands, the caret after its last character.
    void focus(const vb::Hit& hit) {
        vb::Box box;
        box.field = hit.args.at(0);
        if (!hit.args.at(1).empty()) box.element = std::stoul(hit.args.at(1));
        const auto value = value_now(box);
        box.text.set(value, value.size());
        shown_.box = std::move(box);
    }
    /// The caret where the box was pressed, at the medium's measured advance, counted from the
    /// scroll the picture drew the box at.
    void place_caret(const vb::Hit& hit, const ws::PaneCanvasPointer& event) {
        const auto metrics = ws::canvas_text_metrics(hand_room());
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

    template <class T>
    void request(const T& value, const std::string& action, loom::Mail& mail, bool again = false) {
        if (asks().count(of_kind(ViewBuilderAsk::Kind::kHost)) >= 16)
            throw std::invalid_argument("16 asks are unanswered; wait for the view host");
        ViewBuilderAsk asked = asking(ViewBuilderAsk::Kind::kHost);
        asked.action = action;
        asked.again = again;
        if (asks().ask(mail, view::kViewHostRole, value, std::move(asked)) != Asks::Asked::kHeld)
            throw std::invalid_argument(action + " could not be queued");
        model_.notice = action + " asked of the view host";
    }
    /// ASK THE VIEW HOST WHETHER THE VIEW STILL RUNS, by resuming it: until it answers, the
    /// builder says it asked.
    void ask_running(loom::Mail& mail) {
        try {
            const auto bytes = view::description_bytes(model_.description);
            request(view::ViewResume{"builder", loom::Bytes(bytes.begin(), bytes.end())}, "resume", mail, true);
            model_.notice = "Asking the view host whether " + model_.description.name + " still runs";
        } catch (const std::exception& e) {
            model_.running = false;
            model_.notice = "Could not ask the view host whether the view still runs: " + std::string(e.what());
        }
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
        ViewBuilderAsk carrying = asking(ViewBuilderAsk::Kind::kCarry);
        carrying.gesture = gesture;
        carrying.label = shape->name();
        carrying.bytes = loom::Bytes(bytes.begin(), bytes.end());
        carrying.drag = drag;
        (void)asks().forget(of_kind(ViewBuilderAsk::Kind::kCarry));
        (void)asks().ask(mail, workshop_role,
                         ws::PaneOperationRequested{vb::kPane, workshop_role, ws::PaneValueCarryRequested::zen_name,
                                                    1, static_cast<std::int64_t>(gesture)},
                         std::move(carrying));
    }
    /// ASK BEFORE A HAND'S SAVE WRITES: a save is class `write`, judged for the actor of the
    /// gesture that delivered it, a key's row or a press, and that gesture's approval is spent on
    /// this save alone. Nothing is written until Workshop allows it, and one save waits at a time.
    /// A File naming nothing is said at once, with nothing asked.
    void ask_save(loom::Mail& mail) {
        if (asks().holds(of_kind(ViewBuilderAsk::Kind::kSave))) {
            model_.notice = "A save is waiting for Workshop's answer; save again once it comes";
            return;
        }
        if (model_.path.empty()) {
            perform("save", {}, mail);
            return;
        }
        ViewBuilderAsk saving = asking(ViewBuilderAsk::Kind::kSave);
        saving.path = model_.path;
        if (asks().approve(mail, std::move(saving), mail.correlation(), {ws::scope::kWrite}) != Asks::Asked::kHeld)
            model_.notice = "Not saved: Workshop could not be asked to allow it";
    }
    /// EXPORT THE SAVED VIEW AS AN APP (app/folder.hpp): judged first, with nothing written; an
    /// app already there waits for a second press; then asked of Workshop as a save is, class
    /// `write` for the hand that pressed it, and written only on its yes. One export waits at a
    /// time.
    void ask_export(loom::Mail& mail) {
        if (asks().holds(of_kind(ViewBuilderAsk::Kind::kExport))) {
            model_.notice = "An export is waiting for Workshop's answer; export again once it comes";
            return;
        }
        if (model_.dirty || file_.empty()) {
            shown_.armed.clear();
            model_.notice = "Not exported: save the view first; an app is made from its saved file";
            return;
        }
        const auto judged = zengine::app::judge_export({project_dir_, file_, zengine::app::app_program()});
        if (!judged.ok) {
            shown_.armed.clear();
            model_.notice = "Not exported: " + judged.refusal;
            return;
        }
        if (judged.exists && shown_.armed != "export") {
            shown_.armed = "export";
            model_.notice = judged.folder + " is there: press Export again to replace the app's files in it; "
                            "its decisions and every other file stay";
            return;
        }
        shown_.armed.clear();
        ViewBuilderAsk exporting = asking(ViewBuilderAsk::Kind::kExport);
        exporting.path = file_;
        exporting.replace = judged.exists;
        if (asks().approve(mail, std::move(exporting), mail.correlation(), {ws::scope::kWrite}) != Asks::Asked::kHeld)
            model_.notice = "Not exported: Workshop could not be asked to allow it";
    }
    /// ...ALLOWED, IT IS JUDGED AGAIN ON THE WORLD AS IT IS NOW and written: the same view's file,
    /// still saved, and no app there that the weaver was not asked about.
    void export_app(const ViewBuilderAsk& asked) {
        if (model_.dirty || file_ != asked.path) {
            model_.notice = "Not exported: the view changed while Workshop was asked; export again";
            return;
        }
        const auto judged = zengine::app::judge_export({project_dir_, file_, zengine::app::app_program()});
        if (!judged.ok) {
            model_.notice = "Not exported: " + judged.refusal;
            return;
        }
        if (judged.exists && !asked.replace) {
            shown_.armed = "export";
            model_.notice = judged.folder + " appeared while Workshop was asked: press Export again to replace "
                            "the app's files in it";
            return;
        }
        const auto written = zengine::app::write_export(judged);
        if (!written.ok) {
            std::string had;
            for (const auto& name : written.written) had += (had.empty() ? "" : ", ") + name;
            model_.notice = "Not exported: " + written.refusal +
                            (had.empty() ? std::string() : " (written before it stopped: " + had + ")");
            return;
        }
        std::string stems;
        for (const auto& stem : judged.stems) stems += (stems.empty() ? "" : ", ") + stem;
        model_.notice = "Exported " + judged.folder + ": start it with its " + zengine::app::kLauncherCmd +
                        " or " + zengine::app::kLauncherSh + "; its console asks about " + stems +
                        " on its first start";
    }
    /// RUN THE APP THE SAVED VIEW WAS EXPORTED AS, kept by nobody: a start is asked of Workshop
    /// for the hand that pressed it as a Save and a Build are, classes `build` and `write`, then of
    /// the launcher by the app's name, which it looks up in the project's `apps/` itself. One start
    /// waits at a time.
    void ask_start(loom::Mail& mail) {
        if (asks().holds(of_kind(ViewBuilderAsk::Kind::kStart)) ||
            asks().holds(of_kind(ViewBuilderAsk::Kind::kLaunch))) {
            model_.notice = "A start is waiting for its answer; run the app again once it comes";
            return;
        }
        if (file_.empty()) {
            model_.notice = "Not started: save the view and export it first; the app is named after its file";
            return;
        }
        ViewBuilderAsk starting = asking(ViewBuilderAsk::Kind::kStart);
        starting.path = std::filesystem::path(file_).stem().string();
        if (asks().approve(mail, std::move(starting), mail.correlation(),
                           {ws::scope::kBuild, ws::scope::kWrite}) != Asks::Asked::kHeld)
            model_.notice = "Not started: Workshop could not be asked to allow it";
    }
    /// ...ALLOWED, THE LAUNCHER IS ASKED, and its answer is said.
    void launch(const std::string& name, loom::Mail& mail) {
        ViewBuilderAsk launching = asking(ViewBuilderAsk::Kind::kLaunch);
        launching.path = name;
        if (asks().ask(mail, zengine::launcher::kLauncherRole,
                       zengine::launcher::Launch{zengine::launcher::kKindApp, name},
                       std::move(launching)) != Asks::Asked::kHeld)
            model_.notice = "Not started: the launcher could not be asked";
        else
            model_.notice = "Starting the app " + name;
    }
    void declare(loom::Mail& mail) {
        this->template facet<Actions>().declare(
            mail, {{"view.save", "Save the view", input::scan::kS, input::mod::kCtrl},
                   {"view.open", "Open a view", input::scan::kO, input::mod::kCtrl},
                   {"view.run", "Run the view", input::scan::kR, input::mod::kCtrl, {},
                    {ws::cause_of<view::ViewRun>()}},
                   {"view.new", "Start a new view", input::scan::kUnknown, input::mod::kNone},
                   {"view.export", "Export the saved view as an app", input::scan::kUnknown,
                    input::mod::kNone},
                   {"view.run-app", "Run the exported app on its own", input::scan::kUnknown,
                    input::mod::kNone, {}, {ws::cause_of<zengine::launcher::Launch>()}},
                   {"view.apply", "Apply to the running view", input::scan::kUnknown,
                    input::mod::kNone, {}, {ws::cause_of<view::ViewApply>()}},
                   {"view.stop", "Stop the running view", input::scan::kUnknown,
                    input::mod::kNone, {}, {ws::cause_of<view::ViewStop>()}},
                   {"view.choices", "More field choices", input::scan::kUnknown,
                    input::mod::kNone}});
    }
    /// MORE'S ACTION: the next field choices, as a press on `[More]` in the picture shown turns
    /// them; nothing while that picture shows no More.
    void more_choices() {
        if (pictures_.empty()) return;
        for (const auto& hit : pictures_.back().hits)
            if (hit.action == "choices" && !hit.args.empty()) {
                shown_.choices_from = std::stoul(hit.args[0]);
                return;
            }
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
    /// SHOW WHAT IT HOLDS NOW, wherever it stands: Workshop's pane and its area's graphics window.
    void show(loom::Mail& mail) {
        show_workshop(mail);
        show_window(mail);
    }
    /// The window that shows the builder's area is sent what changed there (nothing when nothing
    /// did), or the picture whole: drawn for the window's room from a copy of the presentation, so
    /// the window never moves what Workshop's pane shows.
    void show_window(loom::Mail& mail) {
        if (!window_.shown() || window_.width() <= 0 || window_.height() <= 0) return;
        const auto room = ws::canvas_window_room(vb::kPane, window_.width(), window_.height(),
                                                 window_.cell_px(), window_.text());
        auto shown = shown_;
        const auto drawn = vb::picture(model_, shown, room, 0);
        window_room_ = room;
        window_shown_ = drawn;
        const auto said = ws::canvas_sketch(vb::named(drawn, model_.description), room);
        std::set<std::string> named;
        for (const auto& record : said.sketch.records()) named.insert(record.action);
        window_parts_.clear();
        for (const auto& hit : drawn.hits)
            if (const auto name = vb::part_name(hit, model_.description); named.count(name) != 0)
                window_parts_.emplace(name, hit);
        window_.draw(said.sketch, mail);
    }
    void show_workshop(loom::Mail& mail) {
        if (room().on_canvas()) {
            auto current = vb::picture(model_, shown_, canvas_room(), ++picture_number_);
            const auto ticket =
                mail.as_role(vb::kRole).send_to_role(workshop_role, vb::named(current, model_.description));
            if (ticket.valid()) {
                pictures_.push_back(std::move(current));
                while (pictures_.size() > 8) pictures_.pop_front();
            }
        } else if (room().prose_rows() > 0 && room().prose_columns() > 0) {
            std::string row = "View Builder: making a view needs a canvas-capable Workshop";
            if (static_cast<std::int64_t>(row.size()) > room().prose_columns())
                row.resize(static_cast<std::size_t>(room().prose_columns()));
            (void)mail.as_role(vb::kRole).send_to_role(workshop_role, ws::PaneContent{vb::kPane, {{row}}});
        }
    }

    Room& room() { return this->template facet<Room>(); }
    const Room& room() const { return this->template facet<Room>(); }
    /// The canvas room this pane holds, or the last one granted (`PaneFacet`).
    const ws::PaneCanvasRoom& canvas_room() const { return room().canvas_room(); }
    /// THE PICTURE A HAND AIMED AT, `picture` of those Workshop's canvas holds, or, for a hand in
    /// the window, the one last drawn there; and the room it stands in.
    const vb::Picture* aimed(std::int64_t picture) const {
        if (window_hand_) return window_shown_ ? &*window_shown_ : nullptr;
        const auto pictured = std::find_if(pictures_.begin(), pictures_.end(),
                                           [&](const auto& p) { return p.content.picture == picture; });
        return pictured == pictures_.end() ? nullptr : &*pictured;
    }
    const ws::PaneCanvasRoom& hand_room() const { return window_hand_ ? window_room_ : canvas_room(); }
    Asks& asks() { return this->template facet<Asks>(); }
    /// This image's one outstanding menu (`PaneMenuFacet`).
    Menu& menu() { return this->template facet<Menu>(); }

    vb::Model model_;
    /// HOW NEAR AN EDGE PLACED BY HAND SNAPS: the layout's `snap-reach`, handed again after a
    /// reload, so never reload state.
    std::int64_t snap_reach_ = vb::kSnapReach;
    vb::Presentation shown_;
    std::optional<Held> held_;
    bool restored_ = false;
    /// WHERE THE PROJECT IS, as `zengine.project` answered; whether that answer is a launch's; the
    /// file the draft was saved to or opened from, never a name only typed into File; what the
    /// project file holds, as last read or written, none while that is not known; and whether it
    /// stands as a launch found it, naming a view file this builder could not open.
    std::string project_dir_;
    bool launch_ = false;
    std::string file_;
    std::optional<std::pair<std::string, bool>> remembered_;
    bool kept_ = false;
    std::int64_t picture_number_ = 0;
    std::deque<vb::Picture> pictures_;
    std::optional<vb::Picture> window_shown_; ///< the picture last drawn in the window, and its room
    ws::PaneCanvasRoom window_room_;
    bool window_hand_ = false; ///< the moment being read is the window's
    ws::WindowDrawing window_{vb::kRole, vb::kRole}; ///< its area in the window that shows it
    std::map<std::string, vb::Hit> window_parts_;   ///< the parts the picture last sent there names
    bool room_asked_ = false;                       ///< its area's room asked in this life
    zengine::component::Clipboard clipboard_;
};
} // namespace
ZEN_EXPORT_WEAVE(ViewBuilderPane)
