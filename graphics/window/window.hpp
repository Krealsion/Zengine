// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_WINDOW_HPP
#define ZENGINE_GRAPHICS_WINDOW_WINDOW_HPP

// A window, over the medium it shows on: the whole job of showing what its areas' owners drew. It
// keeps each area's nodes, records and presented elements in its state, presents an element from
// code it loads, has what changed painted with the area's skin, routes a press, gives keys and
// their text to the areas that hear them, and tells each owner its room. What it keeps painted,
// how it shows it and what it reads are its Medium's: characters on a terminal
// (graphics/window/window_terminal.hpp), light in an OS window (graphics/window/window_sdl.hpp).

#include "graphics/painting.hpp"
#include "graphics/vocabulary.hpp"
#include "graphics/window/compose.hpp"
#include "graphics/window/seams.hpp"
#include "graphics/window/store.hpp"

#include "input/translate.hpp"
#include "input/vocabulary.hpp"
#include "manual/vocabulary.hpp"
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
#include <utility>
#include <variant>
#include <vector>

namespace zengine::graphics::window {

/// How often the window reads its medium: the Input weave's own beat.
inline constexpr const char* kPumpTimerId = "zengine.window.pump";
inline constexpr std::int64_t kPumpBeatMs = 10;

/// What a window keeps across a reload: its incarnation, its areas with everything drawn in them,
/// the element code offered for each type, and the next handle it names an element by.
struct WindowState {
    std::int64_t epoch = 0;
    std::vector<WinArea> areas;
    std::vector<ElementCodeOffered> codes;
    std::int64_t next_handle = 1;
    std::int64_t refusals = 0;
    ZEN_SHAPE(WindowState, 2, ZEN_FIELD(epoch), ZEN_FIELD(areas), ZEN_FIELD(codes),
              ZEN_FIELD(next_handle), ZEN_FIELD(refusals));
};

/// A window's size in cells.
struct CellExtent {
    std::int64_t cols = 0, rows = 0;
};

template <class Medium>
class Window;

template <class Medium>
using WindowBase = typename zengine::With<
    zengine::manual::ManualFacet<Medium::kManual>>::template Weave<Window<Medium>, WindowState,
    loom::Accept<Drawn, ElementPut, ElementChange, AreaIntroduced, AreaWithdrawn,
                 ElementCodeOffered, GraphicsReady, WindowShowAsked>,
    loom::Emit<DrawnRefused, WindowOffered, AreaRoom, zengine::input::KeyPressed,
               zengine::input::TextEntered, ActionRequested, zengine::input::PointerButton,
               WindowShown>,
    zengine::timer::TimedWeave>;

/// The window over `Medium`, which names it (`kWindow`), carries its manual (`kManual`), and
/// keeps, paints, shows, lands and reads what the window asks of it.
template <class Medium>
class Window : public WindowBase<Medium> {
public:
    using WindowBase<Medium>::on;

    Window() : pump_(declare_pump()) {}
    template <class Device>
    explicit Window(Device device) : medium_(std::move(device)), pump_(declare_pump()) {}

    // ---- drawing -------------------------------------------------------------------------

    void on(const Drawn& d, loom::Mail& mail) {
        WinArea* a = find(d.area);
        if (a == nullptr || !mail.authored_from_role(a->owner)) {
            refuse(mail, d.area,
                   a == nullptr ? "window '" + std::string(Medium::kWindow) +
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
        const bool repaint = a->library != i.library || a->skin != i.skin;
        const bool handed = a->owner != i.owner;
        const std::string old_library = a->library;
        if (moved || repaint || handed) {
            medium_.cover_area(*a);
        }
        if (handed) {
            medium_.drop_area(a->area);
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
        a->pointer = i.pointer;
        a->skin = i.skin;
        a->library = i.library;
        if (repaint) {
            remake(*a);
            close_unused(old_library);
        } else if (moved) {
            medium_.cover_area(*a);
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
        medium_.cover_area(*a);
        const std::string library = a->library;
        medium_.drop_area(w.area);
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
        s.window = Medium::kWindow;
        s.epoch = this->state_.epoch;
        s.rows = medium_.rows_shown(this->state_.areas);
        s.areas = static_cast<std::int64_t>(this->state_.areas.size());
        for (const WinArea& a : this->state_.areas) {
            s.nodes += static_cast<std::int64_t>(a.nodes.size());
            s.elements += static_cast<std::int64_t>(a.presented.size());
        }
        s.showings = medium_.showings();
        s.device_bytes = medium_.device_bytes();
        s.painter_calls = painters_.calls;
        s.element_calls = codes_.calls;
        s.refusals = this->state_.refusals;
        s.last_refusal = last_refusal_;
        (void)mail.answer(s);
    }

    // ---- life ---------------------------------------------------------------------------------

    /// Started, or started again: a fresh window draws its incarnation (a reload carries the one
    /// it had), takes its medium, paints what its state keeps, shows it, and offers itself.
    void on_timed_activation(const loom::Activated&, loom::Mail& mail) {
        if (this->state_.epoch == 0) {
            this->state_.epoch = token();
        }
        medium_.claim();
        medium_.clear();
        for (WinArea& a : this->state_.areas) {
            remake(a);
        }
        settle(false);
        offer(mail);
    }

private:
    auto declare_pump() {
        return this->timers().repeat(kPumpTimerId, std::chrono::milliseconds(kPumpBeatMs),
                                     &Window::on_pump);
    }

    // ---- input --------------------------------------------------------------------------------

    /// The window's own beat: its medium measured, and what it read sent on. A key and the text
    /// it typed go alike, to the owner of each area placed to hear keys.
    void on_pump(const zengine::timer::TimerFired&, loom::Mail& mail) {
        if (medium_.remeasure()) {
            offer(mail);
        }
        for (const zengine::input::InputEvent& e : medium_.poll()) {
            if (const auto* k = std::get_if<zengine::input::KeyPressed>(&e)) {
                to_keys(mail, *k);
            } else if (const auto* t = std::get_if<zengine::input::TextEntered>(&e)) {
                to_keys(mail, *t);
            } else if (const auto* b = std::get_if<zengine::input::PointerButton>(&e)) {
                if (b->pressed && b->button == 1) {
                    press(mail, *b);
                }
            }
        }
        settle(false);
    }

    /// A key, or its text, to the owner of each area placed to hear keys, front to back.
    template <class Typed>
    void to_keys(loom::Mail& mail, const Typed& typed) {
        for (const WinArea* a : front_to_back(this->state_.areas)) {
            if (a->keys && !a->owner.empty()) {
                (void)mail.as_role(office()).send_to_role(a->owner, typed);
            }
        }
    }

    /// A press where the medium lands it: the element presented there, if its code takes it; else
    /// the action its record names, to the area's owner; else, to an owner whose area was placed
    /// to hear the pointer, the press itself at the area's own point; else nothing.
    void press(loom::Mail& mail, const zengine::input::PointerButton& b) {
        const Landed l = medium_.land(this->state_.areas, b);
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
        if (a->pointer) {
            zengine::input::PointerButton at = b;
            at.x = l.x;
            at.y = l.y;
            at.space = zengine::input::space::kPixels;
            (void)mail.as_role(office()).send_to_role(a->owner, at);
        }
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
        for (const KeptNode& k : a.nodes) {
            if (touches(a, who, d, k.node.path)) {
                medium_.cover(a, k.node.path);
            }
        }
        const Applied applied = apply(a, who, d);
        for (const std::string& gone : applied.gone) {
            medium_.drop(a, gone);
        }
        paint(a, applied.changed);
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

    /// Some of an area's nodes painted by its medium, each sent with every ancestor present (a
    /// node is drawn in their space); a skin that will not paint is the last refusal.
    void paint(const WinArea& a, const std::set<std::string>& paths) {
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
        if (send.empty()) {
            return;
        }
        PaintTask task;
        for (const KeptNode& k : a.nodes) {
            if (send.count(k.node.path) != 0) {
                task.nodes.push_back(
                    PaintNode{k.node.path, key_of(at, k.node.path), k.node.op, k.node.args});
            }
        }
        std::string why;
        medium_.paint(painters_, a, task, paths, why);
        if (!why.empty()) {
            note("area '" + a.area + "': " + why);
        }
    }

    /// Paint all of an area afresh.
    void remake(const WinArea& a) {
        medium_.cover_area(a);
        medium_.drop_area(a.area);
        std::set<std::string> all;
        for (const KeptNode& k : a.nodes) {
            all.insert(k.node.path);
        }
        paint(a, all);
    }

    /// A drawer's word decides when its beat's drawing shows: one saying more follows holds the
    /// showing back; any other handler shows what it changed.
    void settle(bool more) {
        if (!more) {
            medium_.show(this->state_.areas);
        }
    }

    // ---- small things ---------------------------------------------------------------------

    static std::string office() { return std::string(kWindowOfficePrefix) + Medium::kWindow; }

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
        medium_.forget(library);
    }

    /// This window, as its own office, to whoever follows it: the directory, and an arranger that
    /// lays out in its room.
    void offer(loom::Mail& mail) {
        const CellExtent size = medium_.extent();
        (void)mail.as_role(office()).publish(
            WindowOffered{Medium::kWindow, size.cols, size.rows, this->state_.epoch});
    }

    /// An area's room, to its owner: this window, its incarnation, the area's size, and the
    /// references of the elements presented there.
    void room(loom::Mail& mail, const WinArea& a) {
        if (a.owner.empty()) {
            return;
        }
        AreaRoom r{a.area, Medium::kWindow, this->state_.epoch, a.w, a.h, {}};
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

    Medium medium_;
    typename WindowBase<Medium>::Handle pump_;
    Painters painters_;
    ElementCodes codes_;
    std::string last_refusal_;
};

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_WINDOW_HPP
