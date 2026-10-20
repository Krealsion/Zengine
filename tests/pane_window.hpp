// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_PANE_WINDOW_HPP
#define ZENGINE_TESTS_PANE_WINDOW_HPP

// A graphics window as a pane's office meets one, for the suites that drive a pane in a window: a
// participant in a `zengine.window.*` office that tells an area its room, keeps what its owner
// draws there by the real window's judge and store (graphics/window/store.hpp), refuses what that
// judge or a case refuses, and gives a press as the real window does: a part's action where the
// area is deaf to the pointer, else the moments themselves. It paints with the character painter,
// not a skin library, and reads an area back as Workshop reads a drawing.

#include "graphics/painting.hpp"
#include "graphics/term_paint.hpp"
#include "graphics/vocabulary.hpp"
#include "graphics/window/compose.hpp"
#include "graphics/window/store.hpp"
#include "workshop/pane_drawing.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace zengine::testing::pane_window {

namespace gfx = zengine::graphics;
namespace gw = zengine::graphics::window;

struct PaneWindowState {
    std::int64_t drawn = 0;
    ZEN_SHAPE(PaneWindowState, 1, ZEN_FIELD(drawn));
};

/// The case's hand inside the window's own delivery, which gives its office a moment to speak in.
struct PaneWindowDo {
    ZEN_SHAPE(PaneWindowDo, 1);
};

/// THE STAND-IN ITSELF: the areas it keeps, every drawing its areas' owners sent it, the refusals
/// it answered, and the drawings it heard from anyone but an area's owner, which it ignores. It
/// holds the platform's clipboard as the real windows do, and answers a paste with what that
/// holds, counting each read.
class PaneWindow
    : public loom::WeaveBase<PaneWindow, PaneWindowState,
                             loom::Accept<gfx::Drawn, PaneWindowDo, surface::ClipboardTextRequested>,
                             loom::Emit<gfx::AreaRoom, gfx::DrawnRefused, gfx::ActionRequested,
                                        gfx::AreaButtonChanged, gfx::AreaPointerMoved,
                                        gfx::AreaWheelTurned, gfx::AreaPressed, gfx::AreaHoldEnded,
                                        gfx::AreaPointerLeft, surface::ClipboardText>> {
public:
    explicit PaneWindow(std::string window) : window_(std::move(window)) {}

    std::string office() const { return std::string(gfx::kWindowOfficePrefix) + window_; }
    const std::string& window() const { return window_; }

    void on(const gfx::Drawn& d, loom::Mail& mail) {
        ++state_.drawn;
        gw::WinArea* a = find(d.area);
        if (a == nullptr || !mail.authored_from_role(a->owner)) {
            ++strangers;
            return;
        }
        heard.push_back(d);
        const std::string why = refuse_next ? std::string("the case refused this drawing")
                                            : gw::judge(*a, gw::Drawer{}, d);
        refuse_next = false;
        if (!why.empty()) {
            refusals.push_back(why);
            (void)mail.as_role(office()).send_to_role(a->owner, gfx::DrawnRefused{d.area, why});
            return;
        }
        (void)gw::apply(*a, gw::Drawer{}, d);
    }

    void on(const PaneWindowDo&, loom::Mail& mail) {
        std::function<void(PaneWindow&, loom::Mail&)> once;
        once.swap(next);
        if (once) {
            once(*this, mail);
        }
    }

    void on(const surface::ClipboardTextRequested&, loom::Mail& mail) {
        ++clipboard_reads;
        (void)mail.answer(surface::ClipboardText{true, platform});
    }

    gw::WinArea* find(const std::string& area) {
        for (gw::WinArea& a : areas) {
            if (a.area == area) {
                return &a;
            }
        }
        return nullptr;
    }

    std::vector<gw::WinArea> areas;
    std::vector<gfx::Drawn> heard;
    std::vector<std::string> refusals;
    int strangers = 0;
    bool refuse_next = false;
    std::function<void(PaneWindow&, loom::Mail&)> next;
    std::string platform;    ///< what the platform clipboard holds
    int clipboard_reads = 0; ///< the pastes asked of this window

private:
    std::string window_;
};

/// A STAND-IN WINDOW ON `bus`, in the office `zengine.window.<window>`, and the case's hand on it:
/// the rooms it tells, what each area shows, and the presses it makes there.
class Window {
public:
    explicit Window(loom::Switchboard& bus, std::string window = "panes") : bus_(bus) {
        auto weave = std::make_unique<PaneWindow>(std::move(window));
        w_ = weave.get();
        loom::Grant grant;
        grant.allow_to_any(gfx::AreaRoom::zen_name, gfx::AreaRoom::zen_version);
        grant.allow_to_any(gfx::DrawnRefused::zen_name, gfx::DrawnRefused::zen_version);
        grant.allow_to_any(gfx::ActionRequested::zen_name, gfx::ActionRequested::zen_version);
        grant.allow_to_any(gfx::AreaButtonChanged::zen_name, gfx::AreaButtonChanged::zen_version);
        grant.allow_to_any(gfx::AreaPointerMoved::zen_name, gfx::AreaPointerMoved::zen_version);
        grant.allow_to_any(gfx::AreaWheelTurned::zen_name, gfx::AreaWheelTurned::zen_version);
        grant.allow_to_any(gfx::AreaPressed::zen_name, gfx::AreaPressed::zen_version);
        grant.allow_to_any(gfx::AreaHoldEnded::zen_name, gfx::AreaHoldEnded::zen_version);
        grant.allow_to_any(gfx::AreaPointerLeft::zen_name, gfx::AreaPointerLeft::zen_version);
        grant.allow_to_any(surface::ClipboardText::zen_name, surface::ClipboardText::zen_version);
        const std::string office = w_->office();
        id_ = bus_.register_weave(std::move(weave), std::move(grant), office);
        w_->zen_set_self(id_);
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    PaneWindow& weave() { return *w_; }
    const std::string& name() const { return w_->window(); }

    /// THE ROOM OF `area`, `cols` by `rows` cells, told its owner as this window's office, empty
    /// while it holds none of the owner's drawing. A new `epoch` is a window started again, which
    /// holds nothing of the area's.
    void room(const std::string& area, const std::string& owner, std::int64_t cols,
              std::int64_t rows, std::int64_t epoch = 1) {
        gw::WinArea* a = w_->find(area);
        if (a == nullptr) {
            w_->areas.push_back(gw::WinArea{});
            a = &w_->areas.back();
            a->area = area;
        } else if (epoch != epochs_[area]) {
            a->nodes.clear();
            a->records.clear();
        }
        epochs_[area] = epoch;
        a->owner = owner;
        a->w = cols * gfx::kCellPx;
        a->h = rows * gfx::kCellPx;
        say(owner, gfx::AreaRoom{area, w_->window(), epoch, a->w, a->h, {},
                                 a->nodes.empty() && a->records.empty()});
    }

    /// WHAT `area` SHOWS, row by row, its nodes in the window's paint order read as Workshop reads
    /// a drawing (`workshop::drawn_rows`), each row's trailing blanks cut.
    std::vector<std::string> rows(const std::string& area) {
        std::vector<std::string> out;
        const gw::WinArea* a = w_->find(area);
        if (a == nullptr) {
            return out;
        }
        const auto at = gw::index_of(*a);
        std::vector<const gw::KeptNode*> order;
        for (const gw::KeptNode& k : a->nodes) {
            order.push_back(&k);
        }
        std::stable_sort(order.begin(), order.end(), [&](const auto* x, const auto* y) {
            return gw::key_of(at, x->node.path) < gw::key_of(at, y->node.path);
        });
        gfx::Sketch sketch;
        for (const gw::KeptNode* k : order) {
            sketch.put(k->node);
        }
        for (const gfx::ElementRecord& r : a->records) {
            sketch.say(r);
        }
        component::RowMap<workshop::DrawnPart> map;
        map.begin();
        for (const surface::SurfaceTextRow& row :
             workshop::drawn_rows(sketch, a->h / gfx::kCellPx, a->w / gfx::kCellPx, map)) {
            const std::size_t end = row.text.find_last_not_of(' ');
            out.push_back(end == std::string::npos ? std::string() : row.text.substr(0, end + 1));
        }
        return out;
    }

    /// The action a press on the cell `col`, `row` of `area` asks, as the real window lands it.
    std::string action_at(const std::string& area, std::int64_t col, std::int64_t row) {
        const gw::WinArea* a = w_->find(area);
        return a == nullptr ? std::string() : landed(*a, painted(*a), col, row);
    }

    /// Every action a press somewhere in `area` asks.
    std::set<std::string> actions(const std::string& area) {
        std::set<std::string> out;
        each_cell(area, [&](const gw::WinArea& a, const gw::Cache& cache, std::int64_t c,
                            std::int64_t r) {
            if (std::string action = landed(a, cache, c, r); !action.empty()) {
                out.insert(std::move(action));
            }
            return false;
        });
        return out;
    }

    /// The first cell, row by row, whose press in `area` asks `action`.
    std::optional<std::pair<std::int64_t, std::int64_t>> cell_of(const std::string& area,
                                                                 const std::string& action) {
        std::optional<std::pair<std::int64_t, std::int64_t>> found;
        each_cell(area, [&](const gw::WinArea& a, const gw::Cache& cache, std::int64_t c,
                            std::int64_t r) {
            if (landed(a, cache, c, r) == action) {
                found = std::make_pair(c, r);
            }
            return found.has_value();
        });
        return found;
    }

    /// A PRESS ON A CELL, as the real window makes one: the action its record names, asked of the
    /// area's owner as this window's office. Returns the action asked, "" for none.
    std::string press(const std::string& area, std::int64_t col, std::int64_t row) {
        std::string action = action_at(area, col, row);
        if (!action.empty()) {
            ask(area, action);
        }
        return action;
    }

    /// ...ON THE PART `action` NAMES, at the first cell asking it: false when none does.
    bool press(const std::string& area, const std::string& action) {
        const auto at = cell_of(area, action);
        return at.has_value() && press(area, at->first, at->second) == action;
    }

    /// ...ON THE FIRST CELL SHOWING `words`, row by row: false, asking nothing, when no row shows
    /// them or nothing drawn there asks anything.
    bool press_words(const std::string& area, const std::string& words) {
        const std::vector<std::string> shown = rows(area);
        for (std::size_t r = 0; r < shown.size(); ++r) {
            const std::size_t c = shown[r].find(words);
            if (c != std::string::npos) {
                return !press(area, static_cast<std::int64_t>(c), static_cast<std::int64_t>(r))
                            .empty();
            }
        }
        return false;
    }

    /// ANY WORDS, asked of the area's owner -- or of `owner`, for an area this window does not
    /// show -- as this window's office, as no press would ask them.
    void ask(const std::string& area, const std::string& action, std::string owner = {}) {
        const gw::WinArea* a = w_->find(area);
        if (owner.empty() && a != nullptr) {
            owner = a->owner;
        }
        say(owner, gfx::ActionRequested{area, action});
    }

    /// WHERE `area` STANDS IN THE WINDOW, in pixels: what a press there tells its arranger.
    void at(const std::string& area, std::int64_t x, std::int64_t y) {
        if (gw::WinArea* a = w_->find(area)) {
            a->x = x;
            a->y = y;
        }
    }

    /// A NUMBER FOR A PRESS, as the window gives one: the press, its moves and its release carry it.
    std::uint64_t next_press() { return ++presses_; }

    /// A BUTTON PRESSED (`down`) OR LET GO AT A CELL OF `area`, as the real window gives an area
    /// placed to hear the pointer: `AreaButtonChanged` at the cell's centre in the area's pixels,
    /// told the area's owner under `press` as this window's office -- and, given an `arranger`,
    /// `AreaPressed` at the window's pixel, told it under the same number.
    void button(const std::string& area, std::int64_t col, std::int64_t row, bool down,
                std::uint64_t press, std::int64_t which = 1, const std::string& arranger = {}) {
        const gw::WinArea* a = w_->find(area);
        const std::string owner = a == nullptr ? std::string() : a->owner;
        const std::int64_t x = col * gfx::kCellPx + gfx::kCellPx / 2;
        const std::int64_t y = row * gfx::kCellPx + gfx::kCellPx / 2;
        say(owner, gfx::AreaButtonChanged{area, which, down, x, y, 0}, press);
        if (!arranger.empty()) {
            const std::int64_t ox = a == nullptr ? 0 : a->x;
            const std::int64_t oy = a == nullptr ? 0 : a->y;
            say(arranger, gfx::AreaPressed{w_->window(), area, which, down, ox + x, oy + y}, press);
        }
    }
    /// A MOVE TO A CELL OF `area`, while `press` holds it or (0) with none held.
    void move(const std::string& area, std::int64_t col, std::int64_t row, std::uint64_t press = 0) {
        const gw::WinArea* a = w_->find(area);
        say(a == nullptr ? std::string() : a->owner,
            gfx::AreaPointerMoved{area, col * gfx::kCellPx + gfx::kCellPx / 2,
                            row * gfx::kCellPx + gfx::kCellPx / 2, 0, 0, 0},
            press);
    }
    /// THE WHEEL AT A CELL OF `area`, `dy` notches.
    void wheel(const std::string& area, std::int64_t col, std::int64_t row, double dy) {
        const gw::WinArea* a = w_->find(area);
        say(a == nullptr ? std::string() : a->owner,
            gfx::AreaWheelTurned{area, 0, dy, col * gfx::kCellPx + gfx::kCellPx / 2,
                           row * gfx::kCellPx + gfx::kCellPx / 2, 0},
            0);
    }
    /// THE HOLD OF `button` IN `area` ENDED with no release, as the real window tells the owner --
    /// and, given an `arranger`, that office too.
    void hold_ended(const std::string& area, std::int64_t button,
                    const std::string& arranger = {}) {
        const gw::WinArea* a = w_->find(area);
        const std::string owner = a == nullptr ? std::string() : a->owner;
        say(owner, gfx::AreaHoldEnded{area, button});
        if (!arranger.empty() && arranger != owner) {
            say(arranger, gfx::AreaHoldEnded{area, button});
        }
    }
    /// THE POINTER LEFT `area`, with no button held, as the real window tells the owner -- and,
    /// given an `arranger`, that office too.
    void left(const std::string& area, const std::string& arranger = {}) {
        const gw::WinArea* a = w_->find(area);
        const std::string owner = a == nullptr ? std::string() : a->owner;
        say(owner, gfx::AreaPointerLeft{area});
        if (!arranger.empty() && arranger != owner) {
            say(arranger, gfx::AreaPointerLeft{area});
        }
    }
    /// A PRESS AND ITS RELEASE AT A CELL OF `area`, under one fresh number.
    void click(const std::string& area, std::int64_t col, std::int64_t row, std::int64_t which = 1,
               const std::string& arranger = {}) {
        const std::uint64_t press = next_press();
        button(area, col, row, true, press, which, arranger);
        button(area, col, row, false, press, which, arranger);
    }
    /// ...ON THE PART `action` NAMES, at the first cell asking it: false when none does.
    bool click(const std::string& area, const std::string& action) {
        const auto at = cell_of(area, action);
        if (!at) {
            return false;
        }
        click(area, at->first, at->second);
        return true;
    }

    /// A DRAWING OF `area` REFUSED, told its owner as this window's office, whatever it last drew.
    void refuse(const std::string& area, const std::string& reason) {
        const gw::WinArea* a = w_->find(area);
        say(a == nullptr ? std::string() : a->owner, gfx::DrawnRefused{area, reason});
    }

private:
    /// Make the window say `shape` to `to` inside its own delivery, and let the bus settle.
    template <class Shape>
    void say(const std::string& to, const Shape& shape, std::uint64_t correlation = 0) {
        w_->next = [to, shape, correlation](PaneWindow& w, loom::Mail& mail) {
            (void)mail.as_role(w.office()).send_to_role(to, shape, correlation);
        };
        (void)bus_.send(id_, loom::Message(loom::to_value(PaneWindowDo{}), loom::WeaveId{},
                                           loom::WeaveId{}, 0));
        bus_.drain_until_idle();
    }

    /// `area`'s nodes painted by the character painter and kept as the window keeps them.
    static gw::Cache painted(const gw::WinArea& a) {
        const auto at = gw::index_of(a);
        gfx::PaintTask task;
        for (const gw::KeptNode& k : a.nodes) {
            task.nodes.push_back(
                gfx::PaintNode{k.node.path, gw::key_of(at, k.node.path), k.node.op, k.node.args});
        }
        const gw::Place pl = gw::place_of(a);
        gw::Cache cache;
        for (const gfx::NodePaint& np : gfx::term::paint_task(task, gfx::term::Look{}).nodes) {
            gw::PaintedNode n;
            n.key = gw::key_of(at, np.path);
            for (const gfx::term::Cell& c : gfx::term::unpack(np.cells)) {
                if (c.col >= 0 && c.row >= 0 && c.col < pl.cols && c.row < pl.rows) {
                    n.cells.push_back(c);
                }
            }
            cache.set(a.area, np.path, std::move(n));
        }
        return cache;
    }

    /// The action of the record a press on that cell lands in, as the window's `press` reads it.
    static std::string landed(const gw::WinArea& a, const gw::Cache& cache, std::int64_t col,
                              std::int64_t row) {
        const std::vector<gw::WinArea> one{a};
        const gw::Landed l = gw::land(one, cache, col, row);
        const gfx::ElementRecord* r = l.area == nullptr ? nullptr : gw::record_of(a, l.path);
        return r == nullptr ? std::string() : r->action;
    }

    /// Each cell of `area`, row by row, painted once, until `visit` says it is done.
    template <class Visit>
    void each_cell(const std::string& area, Visit&& visit) {
        const gw::WinArea* a = w_->find(area);
        if (a == nullptr) {
            return;
        }
        const gw::Cache cache = painted(*a);
        for (std::int64_t r = 0; r < a->h / gfx::kCellPx; ++r) {
            for (std::int64_t c = 0; c < a->w / gfx::kCellPx; ++c) {
                if (visit(*a, cache, c, r)) {
                    return;
                }
            }
        }
    }

    loom::Switchboard& bus_;
    PaneWindow* w_ = nullptr;
    loom::WeaveId id_{};
    std::map<std::string, std::int64_t> epochs_;
    std::uint64_t presses_ = std::uint64_t{1} << 40; ///< a press's number, past any a desk mints
};

/// THE GRAPHICS DIRECTORY AS AN OWNER MEETS IT: the `zengine.graphics` office, keeping each room
/// asked of it and the office that asked, answering none.
class PaneDirectory
    : public loom::WeaveBase<PaneDirectory, PaneWindowState, loom::Accept<gfx::RoomAsked>,
                             loom::Emit<>> {
public:
    struct Asked {
        std::string area, office;
    };
    void on(const gfx::RoomAsked& r, loom::Mail& mail) {
        asked.push_back(Asked{r.area, std::string(mail.authored_role())});
    }
    /// How many times `office` asked the room of `area`.
    std::size_t times(const std::string& area, const std::string& office) const {
        std::size_t n = 0;
        for (const Asked& a : asked) {
            n += a.area == area && a.office == office ? std::size_t{1} : std::size_t{0};
        }
        return n;
    }
    std::vector<Asked> asked;
};

/// A stand-in directory mounted on `bus`, holding the graphics directory's office.
inline PaneDirectory* mount_directory(loom::Switchboard& bus) {
    auto weave = std::make_unique<PaneDirectory>();
    PaneDirectory* raw = weave.get();
    raw->zen_set_self(bus.register_weave(std::move(weave), loom::Grant{}, gfx::kGraphicsRole));
    return raw;
}

} // namespace zengine::testing::pane_window

#endif // ZENGINE_TESTS_PANE_WINDOW_HPP
