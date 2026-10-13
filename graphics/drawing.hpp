// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_DRAWING_HPP
#define ZENGINE_GRAPHICS_DRAWING_HPP

// A drawing weave: the room's handshake and the showing, owned here so that a drawer writes its
// picture and what it reacts to. A weave opts in as it opts into the Timer's `TimedWeave`, and over
// one: `DrawingWeave<Self, State, Accept<...>, Emit<...>, zengine::timer::TimedWeave>`. A drawer
// that keeps a handshake of its own derives `loom::WeaveBase` and draws with a `Canvas`.
// Reference: graphics/docs/graphics.md, "A drawing weave".

#include "graphics/canvas.hpp"
#include "graphics/vocabulary.hpp"

#include <zen/weave.hpp>

#include <string>
#include <type_traits>
#include <utility>

namespace zengine::graphics {

namespace detail {

/// A base's handlers, in view of the drawing weave above it. Loom's `WeaveBase` declares none, so
/// it is given one no message selects, and a drawer's one `using DrawingWeave::on;` names them all.
template <class B>
struct BaseHandlers : B {
    using B::on;
};
template <class S, class St, class Ac, class Em, class Cl>
struct BaseHandlers<loom::WeaveBase<S, St, Ac, Em, Cl>> : loom::WeaveBase<S, St, Ac, Em, Cl> {
    void on() = delete;
};

} // namespace detail

/// The authoring base of a drawer: `Base` -- Loom's `WeaveBase`, or a base of the same parameters
/// such as the Timer's `TimedWeave` -- with `AreaRoom` and `DrawnRefused` accepted and `Drawn` and
/// `RoomAsked` emitted ahead of the drawer's own. A drawer that presents elements adds
/// `ElementPut` and `ElementChange` to its own `Emit`, and the presses it wants to its `Accept`.
template <class Self, class State, class AcceptList, class EmitList = loom::Emit<>,
          template <class...> class Base = loom::WeaveBase>
class DrawingWeave;

template <class Self, class State, class... A, class... E, template <class...> class Base>
class DrawingWeave<Self, State, loom::Accept<A...>, loom::Emit<E...>, Base>
    : public detail::BaseHandlers<Base<Self, State, loom::Accept<AreaRoom, DrawnRefused, A...>,
                                       loom::Emit<Drawn, RoomAsked, E...>>> {
    using Handlers = detail::BaseHandlers<
        Base<Self, State, loom::Accept<AreaRoom, DrawnRefused, A...>,
             loom::Emit<Drawn, RoomAsked, E...>>>;

public:
    using Handlers::on;

    /// The room, from the window that shows the area, as that window's office. When the window or
    /// its incarnation changed, the drawer's `on_room` runs (an element the window does not hold
    /// is said there), and its picture goes whole at the end of the delivery.
    void on(const AreaRoom& r, loom::Mail& mail) {
        if (r.area != canvas_.area() ||
            !mail.authored_from_role(std::string(kWindowOfficePrefix) + r.window)) {
            return;
        }
        if (canvas_.room(r)) {
            if constexpr (requires(Self& s, const AreaRoom& a, loom::Mail& m) {
                              s.on_room(a, m);
                          }) {
                static_cast<Self*>(this)->on_room(r, mail);
            }
        }
    }

    /// A drawing the window refused, in words: what it shows stays as it was. A drawer that wants
    /// the words declares this handler itself.
    void on(const DrawnRefused&, loom::Mail&) {}

    /// Loom's end-of-delivery hook, after every handler and every poke that wrote the state: the
    /// drawer's picture drawn (only what changed is sent), the room asked once in each life while
    /// no window has said it, and the beat shown -- its nodes, then its elements, shown once.
    void after_delivery(loom::Mail& mail) {
        if constexpr (requires(Self& s, Sketch& f) { s.picture(f); }) {
            Sketch picture;
            static_cast<Self*>(this)->picture(picture);
            canvas_.draw(picture);
        }
        if (!asked_ && canvas_.window().empty()) {
            asked_ = true;
            (void)mail.as_role(canvas_.office())
                .send_to_role(kGraphicsRole, RoomAsked{canvas_.area()});
        }
        canvas_.show(mail);
    }

    // ---- the drawer's two hooks, each optional and public ------------------------------------
    //
    // `void picture(zengine::graphics::Sketch&) const;` -- the drawer's nodes, drawn from its state
    // at the end of every delivery. `void on_room(const AreaRoom&, loom::Mail&);` -- the window or
    // its incarnation changed. A private hook is indistinguishable from an absent one.

protected:
    /// `office` is the role the drawer holds and draws as; `area` its area, the office by default.
    explicit DrawingWeave(std::string office, std::string area = {})
        : canvas_(std::move(office), std::move(area)) {
        static_assert(
            std::is_same_v<decltype(&Self::after_delivery), void (DrawingWeave::*)(loom::Mail&)>,
            "zengine::graphics::DrawingWeave: this drawer defines its own after_delivery, which "
            "REPLACES the drawing weave's: its picture would never be drawn and nothing it queued "
            "would be shown. Do that work in a handler, or keep a handshake of your own: derive "
            "loom::WeaveBase and draw with a zengine::graphics::Canvas.");
    }

    /// What the drawer draws with: `present` and `change` an element; `holds` asks whether the
    /// window presents one.
    Canvas& canvas() { return canvas_; }
    const Canvas& canvas() const { return canvas_; }

private:
    Canvas canvas_;
    bool asked_ = false; ///< the room asked in this life
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_DRAWING_HPP
