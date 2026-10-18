// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_PAINTER_EXPORT_HPP
#define ZENGINE_GRAPHICS_PAINTER_EXPORT_HPP

// Exporting a skin behind the painter seam: a shared painter and a look -- the character painter
// (graphics/term_paint.hpp) with a `term::Look`, or the painter for light
// (graphics/light_paint.hpp) with a `light::Look`. Ours and a stranger's are exported alike:
//
//   struct Neon { static constexpr const char* kName = "neon"; static term::Look look(); };
//   ZENGINE_EXPORT_PAINTER(Neon)
//
//   struct Dusk { static constexpr const char* kName = "dusk"; static light::Look look(); };
//   ZENGINE_EXPORT_LIGHT_PAINTER(Dusk)

#include "graphics/light_paint.hpp"
#include "graphics/painter_abi.h"
#include "graphics/term_paint.hpp"

#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

namespace zengine::graphics::painter_export {

inline int refuse(ZenByteSink reason, const std::string& why, int status) {
    reason.write(reason.ctx, reinterpret_cast<const std::uint8_t*>(why.data()), why.size());
    return status;
}

/// A look's medium: characters, painted into cells.
template <class L>
struct Characters {
    static PaintResult paint(const PaintTask& t) { return term::paint_task(t, L::look()); }
};

/// A look's medium: light, painted into pixel primitives.
template <class L>
struct Light {
    static PaintResult paint(const PaintTask& t) { return light::paint_task(t, L::look()); }
};

/// No exception crosses the seam: a refusal is its words and a status.
template <class Medium>
int paint(const std::uint8_t* task, std::size_t task_len, ZenByteSink result,
          ZenByteSink reason) {
    try {
        const auto t = loom::claim_item<PaintTask>(loom::Bytes(task, task + task_len));
        if (!t) {
            return refuse(reason, "the task does not read as a PaintTask", 1);
        }
        const loom::Bytes b = loom::bequeath_item(Medium::paint(*t));
        result.write(result.ctx, b.data(), b.size());
        return 0;
    } catch (const std::exception& ex) {
        return refuse(reason, ex.what(), 2);
    }
}

} // namespace zengine::graphics::painter_export

#define ZENGINE_EXPORT_PAINTER_FOR(LOOK, MEDIUM)                                                   \
    extern "C" {                                                                                   \
    ZEN_KERNEL_EXPORT const ZenginePainterV1* zengine_painter(void) {                              \
        static const ZenginePainterV1 table{                                                       \
            ZENGINE_PAINTER_ABI_VERSION, LOOK::kName,                                              \
            &::zengine::graphics::painter_export::paint<MEDIUM<LOOK>>};                            \
        return &table;                                                                             \
    }                                                                                              \
    }

/// A skin for a medium of characters: the character painter and LOOK, a `term::Look`.
#define ZENGINE_EXPORT_PAINTER(LOOK)                                                               \
    ZENGINE_EXPORT_PAINTER_FOR(LOOK, ::zengine::graphics::painter_export::Characters)

/// A skin for a medium of light: the painter for light and LOOK, a `light::Look`.
#define ZENGINE_EXPORT_LIGHT_PAINTER(LOOK)                                                         \
    ZENGINE_EXPORT_PAINTER_FOR(LOOK, ::zengine::graphics::painter_export::Light)

#endif // ZENGINE_GRAPHICS_PAINTER_EXPORT_HPP
