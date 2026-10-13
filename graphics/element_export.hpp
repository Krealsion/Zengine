// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_ELEMENT_EXPORT_HPP
#define ZENGINE_GRAPHICS_ELEMENT_EXPORT_HPP

// Exporting an element's code behind the element seam: a struct naming its data form, its state
// and the changes it takes (Loom shapes, the changes in index order), with three static functions
// over its state, and one macro, `ZENGINE_EXPORT_ELEMENT(Code)`. The code draws its whole element
// from its state each time; the window is handed the difference from the drawing of the state
// before, so only what changed is painted. Code draws what it is told: the rules stay its owner's.
// Reference: graphics/docs/graphics.md, "An element of your own".

#include "graphics/canvas.hpp"
#include "graphics/element_abi.h"
#include "graphics/implied.hpp"
#include "graphics/painting.hpp"

#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace zengine::graphics::element_export {

inline int refuse(ZenByteSink reason, const std::string& why, int status) {
    reason.write(reason.ctx, reinterpret_cast<const std::uint8_t*>(why.data()), why.size());
    return status;
}

/// A change's body read as the change of that index and handed to the code: false when the index
/// names no change this code takes, or the body does not read as it.
template <class E, std::size_t... I>
bool change(typename E::State& state, std::int64_t kind, const loom::Bytes& body,
            std::index_sequence<I...>) {
    bool done = false;
    ((static_cast<std::int64_t>(I) == kind
          ? (done =
                 [&] {
                     auto c = from_body<std::tuple_element_t<I, typename E::Changes>>(body);
                     if (!c) {
                         return false;
                     }
                     E::change(state, *c);
                     return true;
                 }(),
             true)
          : false) ||
     ...);
    return done;
}

template <class Tuple, std::size_t... I>
std::vector<std::string> names(std::index_sequence<I...>) {
    return {loom::schema_of<std::tuple_element_t<I, Tuple>>()->name()...};
}

template <class E>
int describe(ZenByteSink described, ZenByteSink reason) {
    try {
        ElementDescribed d;
        d.type = loom::schema_of<typename E::Data>()->name();
        d.changes = names<typename E::Changes>(
            std::make_index_sequence<std::tuple_size_v<typename E::Changes>>{});
        const loom::Bytes b = loom::bequeath_item(d);
        described.write(described.ctx, b.data(), b.size());
        return 0;
    } catch (const std::exception& ex) {
        return refuse(reason, ex.what(), 2);
    }
}

/// No exception crosses the seam: a refusal is its words and a status, and the window keeps the
/// element as it was.
template <class E>
int act(const std::uint8_t* event, std::size_t event_len, ZenByteSink outcome,
        ZenByteSink reason) {
    try {
        const auto e = loom::claim_item<ElementEvent>(loom::Bytes(event, event + event_len));
        if (!e) {
            return refuse(reason, "the event does not read as an ElementEvent", 1);
        }
        typename E::State state{};
        const bool had = !e->state.empty();
        if (had) {
            auto s = loom::claim_item<typename E::State>(e->state);
            if (!s) {
                return refuse(reason, "the state the window kept does not read as this code's", 1);
            }
            state = *s;
        }
        Sketch before;
        if (had && e->kind != event::kRedraw) {
            E::draw(state, before, e->path);
        }
        ElementOutcome out;
        if (e->kind == event::kPut) {
            auto d = loom::claim_item<typename E::Data>(e->input);
            if (!d) {
                return refuse(reason, "its data does not read as " +
                                          loom::schema_of<typename E::Data>()->name(),
                              1);
            }
            state = E::start(*d);
        } else if (e->kind == event::kChange) {
            if (!had || !change<E>(state, e->change_kind, e->input,
                                   std::make_index_sequence<
                                       std::tuple_size_v<typename E::Changes>>{})) {
                return refuse(reason, "change " + std::to_string(e->change_kind) +
                                          " is none this code takes, or its body does not read",
                              1);
            }
        } else if (e->kind == event::kPress) {
            out.took = had && E::press(state, e->node, e->path);
        } else if (e->kind != event::kRedraw) {
            return refuse(reason, "no event '" + e->kind + "'", 1);
        }
        Sketch after;
        E::draw(state, after, e->path);
        out.drawn = difference(before, after);
        out.drawn.whole = !had || e->kind == event::kRedraw || e->kind == event::kPut;
        out.state = loom::bequeath_item(state);
        const loom::Bytes b = loom::bequeath_item(out);
        outcome.write(outcome.ctx, b.data(), b.size());
        return 0;
    } catch (const std::exception& ex) {
        return refuse(reason, ex.what(), 2);
    }
}

} // namespace zengine::graphics::element_export

#define ZENGINE_EXPORT_ELEMENT(CODE)                                                               \
    extern "C" {                                                                                   \
    ZEN_KERNEL_EXPORT const ZengineElementV1* zengine_element(void) {                              \
        static const std::string type = loom::schema_of<CODE::Data>()->name();                    \
        static const ZengineElementV1 table{ZENGINE_ELEMENT_ABI_VERSION, type.c_str(),             \
                                            &::zengine::graphics::element_export::describe<CODE>,  \
                                            &::zengine::graphics::element_export::act<CODE>};      \
        return &table;                                                                             \
    }                                                                                              \
    }

#endif // ZENGINE_GRAPHICS_ELEMENT_EXPORT_HPP
