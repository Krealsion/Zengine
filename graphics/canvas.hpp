// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_CANVAS_HPP
#define ZENGINE_GRAPHICS_CANVAS_HPP

// What an owner draws with. A Sketch collects one picture's drawing calls; a Canvas, kept for the
// owner's life, remembers what it last sent its window, so a whole picture drawn again sends only
// what changed. A beat queues its drawing (`draw`) and its elements' changes (`change`), and `show`
// sends them, the last saying the beat's drawing is whole, so the window shows once; a drawing
// weave (graphics/drawing.hpp) shows at the end of every delivery. Element code draws into a Sketch
// too (graphics/element_export.hpp). Reference: graphics/docs/graphics.md.

#include "graphics/implied.hpp"
#include "graphics/ops.hpp"
#include "graphics/vocabulary.hpp"

#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace zengine::graphics {

/// One picture's drawing calls, collected: nodes in the order drawn, the element records beside
/// them, and the names removed.
class Sketch {
public:
    void group(const std::string& name, std::int64_t x, std::int64_t y, std::int64_t clip_w = 0,
               std::int64_t clip_h = 0) {
        put(name, op::kGroup, GroupArgs{x, y, clip_w, clip_h});
    }
    void rect(const std::string& name, std::int64_t x, std::int64_t y, std::int64_t w,
              std::int64_t h, Paint fill, Paint stroke = no_paint()) {
        put(name, op::kRect, RectArgs{x, y, w, h, fill, stroke});
    }
    void fill_rect(const std::string& name, std::int64_t x, std::int64_t y, std::int64_t w,
                   std::int64_t h, Paint paint) {
        rect(name, x, y, w, h, paint, no_paint());
    }
    void outline_rect(const std::string& name, std::int64_t x, std::int64_t y, std::int64_t w,
                      std::int64_t h, Paint paint) {
        rect(name, x, y, w, h, no_paint(), paint);
    }
    void ellipse(const std::string& name, std::int64_t x, std::int64_t y, std::int64_t w,
                 std::int64_t h, Paint fill, Paint stroke = no_paint()) {
        put(name, op::kEllipse, EllipseArgs{x, y, w, h, fill, stroke});
    }
    void line(const std::string& name, std::int64_t x1, std::int64_t y1, std::int64_t x2,
              std::int64_t y2, Paint ink) {
        put(name, op::kLine, LineArgs{x1, y1, x2, y2, ink});
    }
    void path(const std::string& name, const std::string& d, Paint fill,
              Paint stroke = no_paint()) {
        put(name, op::kPath, PathArgs{d, fill, stroke});
    }
    void text(const std::string& name, std::int64_t x, std::int64_t y, const std::string& words,
              Paint ink) {
        put(name, op::kText, TextArgs{x, y, words, ink});
    }
    void image(const std::string& name, std::int64_t x, std::int64_t y, std::int64_t w,
               std::int64_t h, std::int64_t pw, std::int64_t ph, loom::Bytes rgba) {
        put(name, op::kImage, ImageArgs{x, y, w, h, pw, ph, std::move(rgba)});
    }

    /// What the subtree at `name` means, as the record shape T's value.
    template <class T>
    void record(const std::string& name, const T& data, const std::string& action = {}) {
        say(ElementRecord{name, loom::schema_of<T>()->name(), loom::bequeath_item(data), action});
    }
    /// What a press anywhere in the subtree at `name` asks of the area's owner.
    void action(const std::string& name, const std::string& action) {
        say(ElementRecord{name, {}, {}, action});
    }

    /// Any operator, by its operands' shape: sent as their body, the operator implying the shape.
    template <class Args>
    void put(const std::string& name, const char* op, const Args& args) {
        put(Node{name, op, body_of(args)});
    }
    void put(Node node) {
        const auto it = at_.find(node.path);
        if (it != at_.end()) {
            drawn_[it->second] = std::move(node);
            return;
        }
        at_[node.path] = drawn_.size();
        drawn_.push_back(std::move(node));
    }
    void say(ElementRecord record) {
        for (ElementRecord& r : said_) {
            if (r.path == record.path) {
                r = std::move(record);
                return;
            }
        }
        said_.push_back(std::move(record));
    }

    const std::vector<Node>& nodes() const { return drawn_; }
    const std::vector<ElementRecord>& records() const { return said_; }

private:
    std::vector<Node> drawn_;
    std::map<std::string, std::size_t> at_;
    std::vector<ElementRecord> said_;
};

/// What changed from one sketch of a thing to the next: nodes new or changed put, in the order
/// drawn; the highest node of each subtree gone removed; records new or changed said; records gone
/// unsaid where their node stays.
inline Drawn difference(const Sketch& before, const Sketch& after) {
    Drawn d;
    std::map<std::string, const Node*> was;
    for (const Node& n : before.nodes()) {
        was[n.path] = &n;
    }
    std::set<std::string> now;
    for (const Node& n : after.nodes()) {
        now.insert(n.path);
        const auto it = was.find(n.path);
        if (it == was.end() || it->second->op != n.op || it->second->args != n.args) {
            d.put.push_back(n);
        }
    }
    for (const Node& n : before.nodes()) {
        if (now.count(n.path) != 0) {
            continue;
        }
        const std::string parent = parent_of(n.path);
        if (parent.empty() || now.count(parent) != 0 || was.count(parent) == 0) {
            d.removed.push_back(n.path);
        }
    }
    std::map<std::string, const ElementRecord*> told;
    for (const ElementRecord& r : before.records()) {
        told[r.path] = &r;
    }
    std::set<std::string> said_now;
    for (const ElementRecord& r : after.records()) {
        said_now.insert(r.path);
        const auto it = told.find(r.path);
        if (it == told.end() || it->second->type != r.type || it->second->data != r.data ||
            it->second->action != r.action) {
            d.said.push_back(r);
        }
    }
    for (const ElementRecord& r : before.records()) {
        if (said_now.count(r.path) == 0 && now.count(r.path) != 0) {
            d.unsaid.push_back(r.path);
        }
    }
    return d;
}

/// What an owner keeps for its life: the office it draws as, its area, the window that shows it
/// and the elements that window presents, what it last sent there, and what one beat has queued.
class Canvas {
public:
    explicit Canvas(std::string office, std::string area = {})
        : office_(std::move(office)), area_(area.empty() ? office_ : std::move(area)) {}

    /// The room the window that shows the area told. True when the window, its incarnation, or
    /// its size changed, or the window says the area is empty: it holds none of this canvas's
    /// nodes, so the next drawing goes whole, and of its elements only those the room names.
    bool room(const AreaRoom& r) {
        if (!r.empty && r.window == window_ && r.epoch == epoch_ && r.w == w_ && r.h == h_) {
            return false;
        }
        window_ = r.window;
        epoch_ = r.epoch;
        w_ = r.w;
        h_ = r.h;
        held_ = std::set<std::int64_t>(r.elements.begin(), r.elements.end());
        known_ = false;
        return true;
    }
    /// Whether the window that shows the area presents the element named `ref`.
    bool holds(std::int64_t ref) const { return held_.count(ref) != 0; }
    const std::string& window() const { return window_; }
    std::int64_t epoch() const { return epoch_; }
    std::int64_t width() const { return w_; }
    std::int64_t height() const { return h_; }
    const std::string& office() const { return office_; }
    const std::string& area() const { return area_; }
    std::string window_office() const { return std::string(kWindowOfficePrefix) + window_; }
    /// The next drawing goes whole.
    void forget() { known_ = false; }

    /// Queue a picture: what changed since the last one sent, or the whole of it when the window
    /// holds none of this canvas's drawing.
    void draw(const Sketch& picture) {
        if (window_.empty()) {
            return; // no window shows the area: the first drawing after its room is whole
        }
        Drawn d;
        if (known_) {
            d = difference(sent_, picture);
        } else {
            d.whole = true;
            d.put = picture.nodes();
            d.said = picture.records();
            known_ = true;
        }
        sent_ = picture;
        d.area = area_;
        if (d.whole || !d.put.empty() || !d.removed.empty() || !d.said.empty() ||
            !d.unsaid.empty()) {
            queued_.emplace_back(std::move(d));
        }
    }

    /// Queue an element to present at `path`, from its data form T, said whole.
    template <class T>
    void present(const std::string& path, std::int64_t ref, const T& data) {
        if (!window_.empty()) {
            queued_.emplace_back(ElementPut{area_, path, loom::schema_of<T>()->name(), ref,
                                            loom::bequeath_item(data), false});
            held_.insert(ref);
        }
    }

    /// Queue a change of the element named `ref`: C's index among `Changes`, the changes its code
    /// takes in order, and C's body alone. A C that is not in `Changes` once does not compile.
    template <class Changes, class C>
    void change(std::int64_t ref, const C& c) {
        if (!window_.empty()) {
            queued_.emplace_back(ElementChange{ref, index_in<C, Changes>(), body_of(c), false});
        }
    }

    /// Send what this beat queued, its nodes before its elements, the last saying the drawing is
    /// whole, so the window shows once. Throws, sending nothing more, when this participant does
    /// not hold the office it draws as.
    void show(loom::Mail& mail) {
        std::vector<Out> out = std::move(queued_);
        queued_.clear();
        std::stable_partition(out.begin(), out.end(),
                              [](const Out& m) { return std::holds_alternative<Drawn>(m); });
        for (std::size_t i = 0; i < out.size(); ++i) {
            const bool more = i + 1 < out.size();
            std::visit(
                [&](auto& m) {
                    m.more = more;
                    const loom::Ticket t =
                        mail.as_role(office_).send_to_role(window_office(), m);
                    if (!t.valid()) {
                        throw std::logic_error("graphics: this participant does not hold the "
                                               "office '" + office_ + "' it draws as");
                    }
                },
                out[i]);
        }
    }

private:
    using Out = std::variant<Drawn, ElementPut, ElementChange>;

    template <class C, class Changes, std::size_t... I>
    static constexpr std::size_t count_of(std::index_sequence<I...>) {
        return (std::size_t{0} + ... +
                (std::is_same_v<C, std::tuple_element_t<I, Changes>> ? std::size_t{1} : 0));
    }
    template <class C, class Changes, std::size_t... I>
    static constexpr std::int64_t index_of(std::index_sequence<I...>) {
        std::int64_t at = -1;
        ((std::is_same_v<C, std::tuple_element_t<I, Changes>> ? (at = static_cast<std::int64_t>(I),
                                                                 true)
                                                              : false) ||
         ...);
        return at;
    }
    template <class C, class Changes>
    static constexpr std::int64_t index_in() {
        constexpr auto all = std::make_index_sequence<std::tuple_size_v<Changes>>{};
        static_assert(count_of<C, Changes>(all) == 1,
                      "zengine::graphics::Canvas::change: this change is not one of the element's "
                      "changes, once -- the `Changes` list its code takes, where a change travels "
                      "as its index");
        return index_of<C, Changes>(all);
    }

    std::string office_;
    std::string area_;
    std::string window_;
    std::int64_t epoch_ = 0;
    std::int64_t w_ = 0;
    std::int64_t h_ = 0;
    std::set<std::int64_t> held_;
    bool known_ = false;
    Sketch sent_;
    std::vector<Out> queued_;
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_CANVAS_HPP
