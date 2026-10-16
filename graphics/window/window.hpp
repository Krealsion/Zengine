// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_WINDOW_HPP
#define ZENGINE_GRAPHICS_WINDOW_WINDOW_HPP

// The terminal window: the whole job of showing what its areas' owners drew, over an injected
// Terminal. It keeps each area's nodes, records and presented elements in its state, presents an
// element from code it loads, paints what changed with the area's skin and keeps it painted,
// composes the areas, reads its terminal, routes a press, gives keys to the areas that hear them,
// and tells each owner its room. Nothing it shows passes through the directory.
// Reference: graphics/docs/zengine.window.terminal.md, its manual, compiled in and answered.

#include "graphics/painting.hpp"
#include "graphics/vocabulary.hpp"
#include "graphics/window/compose.hpp"
#include "graphics/window/seams.hpp"
#include "graphics/window/store.hpp"

#include "input/translate.hpp"
#include "input/vocabulary.hpp"
#include "manual/vocabulary.hpp"
#include "manuals/zengine.window.terminal.hpp" // this office's manual, compiled in from its page
#include "surface/terminal_size.hpp"
#include "surface/tui_terminal.hpp"
#include "timer/binding.hpp"

#include <zen/serialize.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace zengine::graphics::window {

/// How often the window reads its terminal: the Input weave's own beat.
inline constexpr const char* kPumpTimerId = "zengine.window.pump";
inline constexpr std::int64_t kPumpBeatMs = 10;
/// The size a window shows when its terminal cannot be measured: a pipe, a suite.
inline constexpr std::int64_t kUnmeasuredCols = 80;
inline constexpr std::int64_t kUnmeasuredRows = 24;

/// What a window keeps across a reload: its incarnation, its areas with everything drawn in them,
/// the element code offered for each type, and the next handle it names an element by.
struct WindowState {
    std::int64_t epoch = 0;
    std::vector<WinArea> areas;
    std::vector<ElementCodeOffered> codes;
    std::int64_t next_handle = 1;
    std::int64_t refusals = 0;
    ZEN_SHAPE(WindowState, 1, ZEN_FIELD(epoch), ZEN_FIELD(areas), ZEN_FIELD(codes),
              ZEN_FIELD(next_handle), ZEN_FIELD(refusals));
};

/// The bytes that show cells on a terminal: for each run, a cursor move (left out where the cursor
/// already stands), the role's ink where it changes, and the glyphs.
inline std::string ansi(const std::vector<term::Cell>& cells) {
    std::string out;
    std::int64_t at_col = -1, at_row = -1, ink = -2;
    for (const term::Cell& c : cells) {
        if (c.col != at_col || c.row != at_row) {
            out += "\x1b[" + std::to_string(c.row + 1) + ";" + std::to_string(c.col + 1) + "H";
        }
        if (c.role != ink) {
            ink = c.role;
            out += surface::sgr_for_role(static_cast<int>(c.role));
        }
        out.push_back(c.glyph);
        at_col = c.col + 1;
        at_row = c.row;
    }
    return out;
}

template <class Terminal>
class WindowT;

template <class Terminal>
using WindowBase = zengine::With<
    zengine::manual::ManualFacet<zengine::manual::pages::zengine_window_terminal::kManual>>::
    Weave<WindowT<Terminal>, WindowState,
          loom::Accept<Drawn, ElementPut, ElementChange, AreaIntroduced, AreaWithdrawn,
                       ElementCodeOffered, GraphicsReady, WindowShowAsked>,
          loom::Emit<DrawnRefused, WindowOffered, AreaRoom, zengine::input::KeyPressed,
                     ElementPressed, ActionRequested, WindowShown>,
          zengine::timer::TimedWeave>;

template <class Terminal>
class WindowT : public WindowBase<Terminal> {
public:
    using WindowBase<Terminal>::on;

    WindowT() : pump_(declare_pump()) {}
    explicit WindowT(Terminal terminal) : terminal_(std::move(terminal)), pump_(declare_pump()) {}

    // ---- drawing -------------------------------------------------------------------------

    void on(const Drawn& d, loom::Mail& mail) {
        WinArea* a = find(d.area);
        if (a == nullptr || !mail.authored_from_role(a->owner)) {
            refuse(mail, d.area,
                   a == nullptr ? "window '" + std::string(kTerminalWindow) +
                                      "' shows no area '" + d.area + "'"
                                : "area '" + d.area + "' is drawn only by its owner, " + a->owner);
            settle(d.more);
            return;
        }
        std::string why;
        if (!draw(*a, Drawer{}, d, why)) {
            refuse(mail, d.area, why);
        }
        settle(d.more);
    }

    void on(const ElementPut& e, loom::Mail& mail) {
        put(mail, e);
        settle(e.more);
    }

    void on(const ElementChange& c, loom::Mail& mail) {
        change(mail, c);
        settle(c.more);
    }

    // ---- what the directory says --------------------------------------------------------------

    /// An area introduced, or introduced again: kept, painted again where it moved or another skin
    /// paints it, emptied when another owner draws in it, and its owner told its room.
    void on(const AreaIntroduced& i, loom::Mail& mail) {
        if (!mail.authored_from_role(kGraphicsRole)) {
            return;
        }
        WinArea* a = find(i.area);
        if (a == nullptr) {
            if (this->state_.areas.size() >= kMaxAreas) {
                note("the window shows " + std::to_string(kMaxAreas) + " areas: '" + i.area +
                     "' is not shown");
                return;
            }
            this->state_.areas.push_back(WinArea{});
            a = &this->state_.areas.back();
            a->area = i.area;
        }
        const bool moved = a->x != i.x || a->y != i.y || a->w != i.w || a->h != i.h ||
                           a->order != i.order;
        const bool repaint = a->library != i.library;
        const bool handed = a->owner != i.owner;
        const std::string old_library = a->library;
        if (moved || repaint || handed) {
            cover_area(*a, dirty_);
        }
        if (handed) {
            cache_.drop_area(a->area);
            a->nodes.clear();
            a->records.clear();
            a->presented.clear();
        }
        a->owner = i.owner;
        a->x = i.x;
        a->y = i.y;
        a->w = i.w;
        a->h = i.h;
        a->order = i.order;
        a->keys = i.keys;
        a->skin = i.skin;
        a->library = i.library;
        if (repaint) {
            remake(*a);
            close_unused(old_library);
        } else if (moved) {
            cover_area(*a, dirty_);
        }
        settle(false);
        room(mail, *a);
    }

    void on(const AreaWithdrawn& w, loom::Mail& mail) {
        if (!mail.authored_from_role(kGraphicsRole)) {
            return;
        }
        WinArea* a = find(w.area);
        if (a == nullptr) {
            return;
        }
        cover_area(*a, dirty_);
        const std::string library = a->library;
        cache_.drop_area(w.area);
        std::erase_if(this->state_.areas, [&](const WinArea& k) { return k.area == w.area; });
        close_unused(library);
        settle(false);
    }

    /// The load plan's element code, through the directory. New code for a type the window
    /// presents draws each such element again, from the state the window kept.
    void on(const ElementCodeOffered& o, loom::Mail& mail) {
        if (!mail.authored_from_role(kGraphicsRole)) {
            return;
        }
        auto& codes = this->state_.codes;
        const auto it = std::find_if(codes.begin(), codes.end(),
                                     [&](const ElementCodeOffered& k) { return k.type == o.type; });
        if (it != codes.end() && it->library == o.library) {
            return;
        }
        const std::string old = it == codes.end() ? std::string() : it->library;
        if (it == codes.end()) {
            codes.push_back(o);
        } else {
            it->library = o.library;
        }
        if (old.empty()) {
            return;
        }
        codes_.close(old);
        for (WinArea& a : this->state_.areas) {
            for (Presented& p : a.presented) {
                if (p.type == o.type) {
                    act(mail, a, p, ElementEvent{event::kRedraw, p.handle, p.path, p.state, 0, {}, {}});
                }
            }
        }
        settle(false);
    }

    /// A new directory: this window says itself again; its areas stay until it says otherwise.
    void on(const GraphicsReady&, loom::Mail& mail) {
        if (mail.authored_from_role(kGraphicsRole)) {
            offer(mail);
        }
    }

    void on(const WindowShowAsked&, loom::Mail& mail) {
        WindowShown s;
        s.window = kTerminalWindow;
        s.epoch = this->state_.epoch;
        s.rows = rows_shown();
        s.areas = static_cast<std::int64_t>(this->state_.areas.size());
        for (const WinArea& a : this->state_.areas) {
            s.nodes += static_cast<std::int64_t>(a.nodes.size());
            s.elements += static_cast<std::int64_t>(a.presented.size());
        }
        s.showings = showings_;
        s.device_bytes = device_bytes_;
        s.painter_calls = painters_.calls;
        s.element_calls = codes_.calls;
        s.refusals = this->state_.refusals;
        s.last_refusal = last_refusal_;
        (void)mail.answer(s);
    }

    // ---- life ---------------------------------------------------------------------------------

    /// Started, or started again: a fresh window draws its incarnation (a reload carries the one
    /// it had), takes its terminal, paints what its state keeps, shows it, and offers itself.
    void on_timed_activation(const loom::Activated&, loom::Mail& mail) {
        if (this->state_.epoch == 0) {
            this->state_.epoch = token();
        }
        terminal_.claim();
        const surface::TerminalSize size = terminal_.size();
        cols_ = size.measured() ? size.cols : kUnmeasuredCols;
        rows_ = size.measured() ? size.rows : kUnmeasuredRows;
        cache_.clear();
        for (WinArea& a : this->state_.areas) {
            remake(a);
        }
        settle(false);
        offer(mail);
    }

private:
    auto declare_pump() {
        return this->timers().repeat(kPumpTimerId, std::chrono::milliseconds(kPumpBeatMs),
                                     &WindowT::on_pump);
    }

    // ---- input --------------------------------------------------------------------------------

    /// The window's own beat: what its terminal typed, read with Zengine's parser.
    void on_pump(const zengine::timer::TimerFired&, loom::Mail& mail) {
        for (const zengine::input::InputEvent& e : terminal_.poll()) {
            if (const auto* k = std::get_if<zengine::input::KeyPressed>(&e)) {
                for (const WinArea* a : front_to_back(this->state_.areas)) {
                    if (a->keys && !a->owner.empty()) {
                        (void)mail.as_role(office()).send_to_role(a->owner, *k);
                    }
                }
            } else if (const auto* b = std::get_if<zengine::input::PointerButton>(&e)) {
                if (b->pressed && b->button == 1) {
                    press(mail, b->x, b->y);
                }
            }
        }
        settle(false);
    }

    /// A press at a cell of the window: the element presented there, if its code takes it; else
    /// the action its record names, to the area's owner; else the press itself, to the drawer.
    void press(loom::Mail& mail, std::int64_t col, std::int64_t row) {
        const Landed l = land(this->state_.areas, cache_, col, row);
        if (l.area == nullptr) {
            return;
        }
        WinArea* a = find(l.area->area);
        Presented* p = presented_at(*a, l.path);
        if (p != nullptr && act(mail, *a, *p,
                                ElementEvent{event::kPress, p->handle, p->path, p->state, 0, {},
                                             l.path})) {
            return;
        }
        const ElementRecord* r = record_of(*a, l.path);
        if (p == nullptr && r != nullptr && !r->action.empty()) {
            (void)mail.as_role(office()).send_to_role(a->owner,
                                                      ActionRequested{a->area, r->action});
            return;
        }
        (void)mail.as_role(office()).send_to_role(
            a->owner, ElementPressed{a->area, p != nullptr ? p->path : (r != nullptr ? r->path : ""),
                                     l.path, l.x, l.y});
    }

    // ---- elements -----------------------------------------------------------------------------

    void put(loom::Mail& mail, const ElementPut& e) {
        WinArea* a = find(e.area);
        if (a == nullptr || !mail.authored_from_role(a->owner)) {
            refuse(mail, e.area, "the sender does not draw in area '" + e.area + "' here");
            return;
        }
        auto it = std::find_if(a->presented.begin(), a->presented.end(),
                               [&](const Presented& p) { return p.path == e.path; });
        if (it == a->presented.end() && !mine(*a, Drawer{}, e.path)) {
            refuse(mail, e.area, "'" + e.path + "' is not the sender's to present an element at");
            return;
        }
        std::string why;
        const ElementCodes::Code* code = code_for(e.type, why);
        if (code == nullptr) {
            refuse(mail, e.area, "element '" + e.path + "': " + why);
            return;
        }
        if (it == a->presented.end()) {
            if (a->presented.size() >= kMaxElementsPerArea) {
                refuse(mail, e.area, "area '" + e.area + "' presents " +
                                         std::to_string(kMaxElementsPerArea) +
                                         " elements already: '" + e.path + "' is not presented");
                return;
            }
            a->presented.push_back(Presented{e.path, e.type, e.ref, this->state_.next_handle++, {}});
            it = std::prev(a->presented.end());
        } else if (it->type != e.type) {
            it->type = e.type;
            it->state.clear();
        }
        Presented trial = *it;
        trial.ref = e.ref;
        if (!act(mail, *a, trial, ElementEvent{event::kPut, trial.handle, e.path, trial.state, 0,
                                               e.data, {}})) {
            if (it->state.empty()) {
                a->presented.erase(it); // never presented: nothing of it stays
            }
            return;
        }
        auto kept = std::find_if(a->presented.begin(), a->presented.end(),
                                 [&](const Presented& p) { return p.path == e.path; });
        *kept = trial;
    }

    void change(loom::Mail& mail, const ElementChange& c) {
        for (WinArea& a : this->state_.areas) {
            if (!mail.authored_from_role(a.owner)) {
                continue;
            }
            for (Presented& p : a.presented) {
                if (p.ref == c.ref) {
                    (void)act(mail, a, p, ElementEvent{event::kChange, p.handle, p.path, p.state,
                                                       c.kind, c.change, {}});
                    return;
                }
            }
        }
        refuse(mail, "", "no element this window presents is the sender's by reference " +
                             std::to_string(c.ref));
    }

    /// The code presenting a type, opened on first use.
    const ElementCodes::Code* code_for(const std::string& type, std::string& why) {
        const auto& codes = this->state_.codes;
        const auto it = std::find_if(codes.begin(), codes.end(),
                                     [&](const ElementCodeOffered& k) { return k.type == type; });
        if (it == codes.end()) {
            why = "no code offered presents '" + type + "'";
            return nullptr;
        }
        return codes_.get(it->library, why);
    }

    /// One event for an element's code: its drawing applied as its own and its state kept, only
    /// when the window accepts the drawing; a refusal is told to the element's drawer. True when
    /// the event was taken.
    bool act(loom::Mail& mail, WinArea& a, Presented& p, const ElementEvent& event) {
        std::string why;
        const ElementCodes::Code* code = code_for(p.type, why);
        if (code == nullptr) {
            refuse_to(mail, a.owner, a.area, "element '" + p.path + "': " + why);
            return false;
        }
        auto out = codes_.act(*code, event, why);
        if (!out) {
            refuse_to(mail, a.owner, a.area, "element '" + p.path + "': " + why);
            return false;
        }
        if (event.kind == event::kPress && !out->took) {
            return false;
        }
        Drawn d = out->drawn;
        d.area = a.area;
        if (d.whole || !d.put.empty() || !d.removed.empty() || !d.said.empty() ||
            !d.unsaid.empty()) {
            if (!draw(a, Drawer{true, p.path}, d, why)) {
                refuse_to(mail, a.owner, a.area, "element '" + p.path + "': " + why);
                return false;
            }
        }
        p.state = out->state;
        return true;
    }

    // ---- painting and showing -------------------------------------------------------------

    /// A drawing judged, applied, and what it changed painted and kept painted.
    bool draw(WinArea& a, const Drawer& who, const Drawn& d, std::string& why) {
        why = judge(a, who, d);
        if (!why.empty()) {
            return false;
        }
        const Place pl = place_of(a);
        for (const KeptNode& k : a.nodes) {
            if (touches(a, who, d, k.node.path)) {
                cache_.cover(a.area, k.node.path, pl, dirty_);
            }
        }
        const Applied applied = apply(a, who, d);
        for (const std::string& gone : applied.gone) {
            cache_.drop(a.area, gone);
        }
        paint_into_cache(a, applied.changed);
        if (d.whole && !who.element) {
            std::erase_if(a.presented, [&](const Presented& p) {
                return index_of(a).count(p.path) == 0 && !applied.changed.count(p.path);
            });
        }
        return true;
    }

    static bool touches(const WinArea& a, const Drawer& who, const Drawn& d,
                        const std::string& path) {
        if (d.whole && mine(a, who, path)) {
            return true;
        }
        for (const std::string& r : d.removed) {
            if (under(path, r)) {
                return true;
            }
        }
        for (const Node& n : d.put) {
            if (under(path, n.path)) {
                return true;
            }
        }
        return false;
    }

    /// Paint some of an area's nodes with its skin, kept painted; their cells marked to show.
    void paint_into_cache(const WinArea& a, const std::set<std::string>& paths) {
        if (paths.empty()) {
            return;
        }
        const auto at = index_of(a);
        std::set<std::string> send;
        for (const std::string& p : paths) {
            if (at.count(p) == 0) {
                continue;
            }
            send.insert(p);
            for (std::string q = parent_of(p); !q.empty(); q = parent_of(q)) {
                if (at.count(q) != 0) {
                    send.insert(q);
                }
            }
        }
        if (send.empty() || a.library.empty()) {
            return;
        }
        std::string why;
        const ZenginePainterV1* skin = painters_.get(a.library, why);
        if (skin == nullptr) {
            note("area '" + a.area + "': " + why);
            return;
        }
        PaintTask task;
        for (const KeptNode& k : a.nodes) {
            if (send.count(k.node.path) != 0) {
                task.nodes.push_back(
                    PaintNode{k.node.path, key_of(at, k.node.path), k.node.op, k.node.args});
            }
        }
        auto result = painters_.paint(skin, task, why);
        if (!result) {
            note("area '" + a.area + "': " + why);
            return;
        }
        const Place pl = place_of(a);
        for (const std::string& p : paths) {
            cache_.drop(a.area, p);
        }
        for (const NodePaint& np : result->nodes) {
            if (paths.count(np.path) == 0 || at.count(np.path) == 0) {
                continue;
            }
            PaintedNode n;
            n.key = key_of(at, np.path);
            for (const term::Cell& c : term::unpack(np.cells)) {
                if (c.col >= 0 && c.row >= 0 && c.col < pl.cols && c.row < pl.rows) {
                    n.cells.push_back(c);
                }
            }
            cache_.set(a.area, np.path, std::move(n));
            cache_.cover(a.area, np.path, pl, dirty_);
        }
    }

    /// Paint all of an area afresh.
    void remake(const WinArea& a) {
        cover_area(a, dirty_);
        cache_.drop_area(a.area);
        std::set<std::string> all;
        for (const KeptNode& k : a.nodes) {
            all.insert(k.node.path);
        }
        paint_into_cache(a, all);
    }

    void cover_area(const WinArea& a, std::set<CellAt>& into) const {
        const Place pl = place_of(a);
        for (const KeptNode& k : a.nodes) {
            cache_.cover(a.area, k.node.path, pl, into);
        }
    }

    /// A drawer's word decides when its beat's drawing shows: one saying more follows holds the
    /// showing back; any other handler shows what it changed.
    void settle(bool more) {
        if (more) {
            return;
        }
        std::vector<term::Cell> cells;
        std::set<CellAt> in_window;
        for (const CellAt& c : dirty_) {
            if (c.first >= 0 && c.second >= 0 && c.first < cols_ && c.second < rows_) {
                in_window.insert(c);
            }
        }
        dirty_.clear();
        if (in_window.empty()) {
            return;
        }
        const std::string bytes = ansi(compose(this->state_.areas, cache_, in_window));
        terminal_.write(bytes);
        ++showings_;
        device_bytes_ += static_cast<std::int64_t>(bytes.size());
    }

    std::vector<std::string> rows_shown() const {
        std::set<CellAt> all;
        for (std::int64_t r = 0; r < rows_; ++r) {
            for (std::int64_t c = 0; c < cols_; ++c) {
                all.insert({c, r});
            }
        }
        std::vector<std::string> rows(static_cast<std::size_t>(rows_),
                                      std::string(static_cast<std::size_t>(cols_), ' '));
        for (const term::Cell& c : compose(this->state_.areas, cache_, all)) {
            rows[static_cast<std::size_t>(c.row)][static_cast<std::size_t>(c.col)] = c.glyph;
        }
        for (std::string& r : rows) {
            r.erase(r.find_last_not_of(' ') + 1);
        }
        return rows;
    }

    // ---- small things ---------------------------------------------------------------------

    static std::string office() { return std::string(kWindowOfficePrefix) + kTerminalWindow; }

    /// A random token of one width, its top bit set: a loaded weave cannot read its own id, and
    /// a window started afresh must differ from every one before it.
    static std::int64_t token() {
        std::random_device seed;
        const std::uint64_t r = (static_cast<std::uint64_t>(seed()) << 32) ^ seed();
        return static_cast<std::int64_t>((r & 0x1fffffffffffffffULL) | (1ULL << 61));
    }

    WinArea* find(const std::string& area) {
        for (WinArea& a : this->state_.areas) {
            if (a.area == area) {
                return &a;
            }
        }
        return nullptr;
    }

    void close_unused(const std::string& library) {
        if (library.empty()) {
            return;
        }
        for (const WinArea& a : this->state_.areas) {
            if (a.library == library) {
                return;
            }
        }
        painters_.close(library);
    }

    void offer(loom::Mail& mail) {
        (void)mail.as_role(office()).send_to_role(
            kGraphicsRole, WindowOffered{kTerminalWindow, cols_, rows_, this->state_.epoch});
    }

    /// An area's room, to its owner: this window, its incarnation, the area's size, and the
    /// references of the elements presented there.
    void room(loom::Mail& mail, const WinArea& a) {
        if (a.owner.empty()) {
            return;
        }
        AreaRoom r{a.area, kTerminalWindow, this->state_.epoch, a.w, a.h, {}};
        for (const Presented& p : a.presented) {
            r.elements.push_back(p.ref);
        }
        (void)mail.as_role(office()).send_to_role(a.owner, r);
    }

    void refuse(loom::Mail& mail, const std::string& area, const std::string& reason) {
        note(reason);
        (void)mail.as_role(office()).send(mail.sender(), DrawnRefused{area, reason});
    }

    void refuse_to(loom::Mail& mail, const std::string& owner, const std::string& area,
                   const std::string& reason) {
        note(reason);
        (void)mail.as_role(office()).send_to_role(owner, DrawnRefused{area, reason});
    }

    void note(const std::string& why) {
        ++this->state_.refusals;
        last_refusal_ = why;
    }

    Terminal terminal_;
    typename WindowBase<Terminal>::Handle pump_;
    Painters painters_;
    ElementCodes codes_;
    Cache cache_;
    std::set<CellAt> dirty_;
    std::int64_t cols_ = kUnmeasuredCols;
    std::int64_t rows_ = kUnmeasuredRows;
    std::int64_t showings_ = 0;
    std::int64_t device_bytes_ = 0;
    std::string last_refusal_;
};

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_WINDOW_HPP
