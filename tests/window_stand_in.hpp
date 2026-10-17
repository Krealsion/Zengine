// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_WINDOW_STAND_IN_HPP
#define ZENGINE_TESTS_WINDOW_STAND_IN_HPP

// A graphics window the pane suites stand in for, on any bus: it holds `zengine.window.<name>`,
// admits each drawing by the window's own rules (graphics/window/store.hpp) or refuses it to its
// drawer as a window does, reads what it holds back as the rows a window of characters shows
// (`workshop::drawn_rows`), and says a room, or the action a press on a cell asks, as its office.

#include "graphics/canvas.hpp"
#include "graphics/vocabulary.hpp"
#include "graphics/window/store.hpp"
#include "workshop/pane_drawing.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace zengine::testing {

struct WindowStandInState {
    std::int64_t heard = 0;
    ZEN_SHAPE(WindowStandInState, 1, ZEN_FIELD(heard));
};

/// The window's office: every drawing it was sent, and each area as the window keeps it.
class WindowStandInSeat
    : public loom::WeaveBase<WindowStandInSeat, WindowStandInState,
                             loom::Accept<graphics::Drawn>,
                             loom::Emit<graphics::AreaRoom, graphics::DrawnRefused,
                                        graphics::ActionRequested>> {
public:
    explicit WindowStandInSeat(std::string office) : office_(std::move(office)) {}

    void on(const graphics::Drawn& d, loom::Mail& mail) {
        ++state_.heard;
        drawn.push_back(d);
        graphics::window::WinArea& a = areas[d.area];
        a.area = d.area;
        const std::string why = graphics::window::judge(a, graphics::window::Drawer{}, d);
        if (!why.empty()) {
            refused.push_back(why);
            (void)mail.as_role(office_).send_to_role(std::string(mail.authored_role()),
                                                     graphics::DrawnRefused{d.area, why});
            return;
        }
        (void)graphics::window::apply(a, graphics::window::Drawer{}, d);
    }

    std::vector<graphics::Drawn> drawn;    ///< every drawing heard, in order
    std::vector<std::string> refused;      ///< why each drawing the window refused was refused
    std::map<std::string, graphics::window::WinArea> areas;

private:
    std::string office_;
};

/// THE STAND-IN ON A BUS, under the window name `name`: the rooms it says, the presses it makes and
/// what its areas show.
class WindowStandIn {
public:
    explicit WindowStandIn(loom::Switchboard& bus, std::string name = "main")
        : bus_(bus), name_(std::move(name)) {
        auto seat = std::make_unique<WindowStandInSeat>(office());
        seat_ = seat.get();
        loom::Grant grant;
        grant.allow_to_any(graphics::AreaRoom::zen_name, graphics::AreaRoom::zen_version);
        grant.allow_to_any(graphics::DrawnRefused::zen_name, graphics::DrawnRefused::zen_version);
        grant.allow_to_any(graphics::ActionRequested::zen_name,
                           graphics::ActionRequested::zen_version);
        id_ = bus_.register_weave(std::move(seat), std::move(grant), office());
        seat_->zen_set_self(id_);
    }
    ~WindowStandIn() { (void)bus_.unregister_weave(id_); }
    WindowStandIn(const WindowStandIn&) = delete;
    WindowStandIn& operator=(const WindowStandIn&) = delete;

    std::string office() const { return std::string(graphics::kWindowOfficePrefix) + name_; }
    WindowStandInSeat& seat() { return *seat_; }

    /// The room of `area`, owned by the office of the same name, `w` by `h` pixels, said as this
    /// window in its incarnation `epoch`.
    void room(const std::string& area, std::int64_t w, std::int64_t h, std::int64_t epoch = 1) {
        rooms_[area] = {w, h};
        say(area, graphics::AreaRoom{area, name_, epoch, w, h, {}});
    }
    /// A press's action, asked of the owner of `area` as this window asks it.
    void ask(const std::string& area, const std::string& action) {
        say(area, graphics::ActionRequested{area, action});
    }
    /// A drawing of `area` refused, as this window tells its owner.
    void refuse(const std::string& area, const std::string& reason) {
        say(area, graphics::DrawnRefused{area, reason});
    }

    /// WHAT `area` SHOWS, row by row, in the cells of the room last said for it.
    std::vector<std::string> shown(const std::string& area) const {
        component::RowMap<workshop::DrawnPart> map;
        std::vector<std::string> out;
        for (const surface::SurfaceTextRow& row : read(area, map)) out.push_back(row.text);
        return out;
    }
    /// THE ACTION A PRESS AT `row`, `column` OF `area` ASKS, as the window finds it: the record of
    /// the part drawn there, or "" where none is.
    std::string action_at(const std::string& area, std::int64_t row, std::int64_t column) const {
        component::RowMap<workshop::DrawnPart> map;
        (void)read(area, map);
        const workshop::DrawnPart* part = map.at(row, column);
        return part == nullptr ? std::string() : part->action;
    }
    /// A PRESS on the first cell showing `words` in `area`: its action asked, as the window asks it.
    /// False, asking nothing, when no row shows them or nothing drawn there asks anything.
    bool press(const std::string& area, const std::string& words) {
        const std::vector<std::string> rows = shown(area);
        for (std::size_t r = 0; r < rows.size(); ++r) {
            const std::size_t at = rows[r].find(words);
            if (at == std::string::npos) continue;
            const std::string action = action_at(area, static_cast<std::int64_t>(r),
                                                 static_cast<std::int64_t>(at));
            if (action.empty()) return false;
            ask(area, action);
            return true;
        }
        return false;
    }

private:
    template <class T>
    void say(const std::string& to, const T& message) {
        (void)bus_.office_send_to_role_as(id_, office(), to, loom::Message(loom::to_value(message)));
        bus_.drain_until_idle();
    }

    /// The nodes the window keeps for `area`, in its paint order, read onto its room's cells.
    std::vector<surface::SurfaceTextRow> read(const std::string& area,
                                              component::RowMap<workshop::DrawnPart>& map) const {
        const auto a = seat_->areas.find(area);
        const auto room = rooms_.find(area);
        if (a == seat_->areas.end() || room == rooms_.end()) return {};
        const auto at = graphics::window::index_of(a->second);
        std::vector<const graphics::window::KeptNode*> order;
        for (const graphics::window::KeptNode& k : a->second.nodes) order.push_back(&k);
        std::stable_sort(order.begin(), order.end(), [&](const auto* x, const auto* y) {
            return graphics::window::key_of(at, x->node.path) <
                   graphics::window::key_of(at, y->node.path);
        });
        graphics::Sketch sketch;
        for (const graphics::window::KeptNode* k : order) sketch.put(k->node);
        for (const graphics::ElementRecord& r : a->second.records) sketch.say(r);
        map.begin();
        return workshop::drawn_rows(sketch, room->second.second / graphics::kCellPx,
                                    room->second.first / graphics::kCellPx, map);
    }

    loom::Switchboard& bus_;
    std::string name_;
    WindowStandInSeat* seat_ = nullptr;
    loom::WeaveId id_{};
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> rooms_; ///< w, h by area
};

} // namespace zengine::testing

#endif // ZENGINE_TESTS_WINDOW_STAND_IN_HPP
