// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_DRAWING_HPP
#define ZENGINE_GRAPHICS_DRAWING_HPP

// A drawing facet (`DrawingFacet`, composed with `With<DrawingFacet>`) and its single-facet
// shorthand `DrawingWeave`: the room's handshake and the showing, owned here so that a drawer
// writes its picture and what it reacts to. With `With<DrawingFacet>`, a drawer's own
// `after_delivery` runs before the facet shows; `DrawingWeave` keeps the single-base wall.
// Reference: graphics/docs/graphics.md, "A drawing weave".

#include "activation/activation.hpp"
#include "graphics/canvas.hpp"
#include "graphics/vocabulary.hpp"

#include <zen/weave.hpp>

#include <string>
#include <type_traits>
#include <utility>

namespace zengine::graphics {

using zengine::With;

/// The facet that owns a drawer's `Canvas`, its room handshake, and its end-of-delivery show.
/// Contributes `Accept<AreaRoom, DrawnRefused>` and `Emit<Drawn, RoomAsked>`, and runs after the
/// weave's own `after_delivery` when composed with `With<DrawingFacet>`.
class DrawingFacet {
public:
    using Accept = loom::Accept<AreaRoom, DrawnRefused>;
    using Emit = loom::Emit<Drawn, RoomAsked>;

    explicit DrawingFacet(std::string office, std::string area = {})
        : canvas_(std::move(office), std::move(area)) {}

    /// The room, from the window that shows the area, as that window's office. When the window,
    /// its incarnation, or its size changed, the drawer's `on_room` runs and its picture goes
    /// whole at the end of the delivery.
    template <class Self>
    void on(Self& self, const AreaRoom& r, loom::Mail& mail) {
        if (r.area != canvas_.area() ||
            !mail.authored_from_role(std::string(kWindowOfficePrefix) + r.window)) {
            return;
        }
        if (canvas_.room(r)) {
            if constexpr (requires(Self& s, const AreaRoom& a, loom::Mail& m) {
                              s.on_room(a, m);
                          }) {
                self.on_room(r, mail);
            }
        }
    }

    /// A drawing the window refused, in words: what it shows stays as it was. A drawer that wants
    /// the words declares this handler itself.
    template <class Self>
    void on(Self&, const DrawnRefused&, loom::Mail&) {}

    /// End-of-delivery step, run after the weave's own `after_delivery` when present: the
    /// drawer's picture drawn, the room asked once in each life while no window has said it,
    /// and the beat shown -- its nodes, then its elements, shown once.
    template <class Self>
    void after_delivery(Self& self, loom::Mail& mail) {
        if constexpr (requires(Self& s, Sketch& f) { s.picture(f); }) {
            Sketch picture;
            self.picture(picture);
            canvas_.draw(picture);
        }
        if (!asked_ && canvas_.window().empty()) {
            asked_ = true;
            (void)mail.as_role(canvas_.office())
                .send_to_role(kGraphicsRole, RoomAsked{canvas_.area()});
        }
        canvas_.show(mail);
    }

    Canvas& canvas() { return canvas_; }
    const Canvas& canvas() const { return canvas_; }

private:
    Canvas canvas_;
    bool asked_ = false; ///< the room asked in this life
};

/// Single-facet shorthand over `With<DrawingFacet>::Weave` that keeps the compile-time refusal on
/// a derived `after_delivery` (compose `With<DrawingFacet>` directly when chaining is wanted).
template <class Self, class State, class AcceptList, class EmitList = loom::Emit<>,
          template <class...> class Base = loom::WeaveBase>
class DrawingWeave
    : public With<DrawingFacet>::template Weave<Self, State, AcceptList, EmitList, Base> {
    using FacetBase =
        typename With<DrawingFacet>::template Weave<Self, State, AcceptList, EmitList, Base>;

public:
    using FacetBase::on;

protected:
    /// `office` is the role the drawer holds and draws as; `area` its area, the office by default.
    explicit DrawingWeave(std::string office, std::string area = {})
        : FacetBase(std::move(office), std::move(area)) {
        static_assert(
            std::is_same_v<decltype(&Self::after_delivery), void (FacetBase::*)(loom::Mail&)>,
            "zengine::graphics::DrawingWeave: this drawer defines its own after_delivery, which "
            "REPLACES the drawing weave's: its picture would never be drawn and nothing it queued "
            "would be shown. Compose With<DrawingFacet> so after_delivery chains, or do that work "
            "in a handler.");
    }
};

} // namespace zengine::graphics

#endif // ZENGINE_GRAPHICS_DRAWING_HPP
